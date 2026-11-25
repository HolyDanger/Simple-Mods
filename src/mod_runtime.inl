static void saveCursor() {
    const auto path = root / L"settings.ini";
    WritePrivateProfileStringW(L"CursorMenu", L"Enabled", cursorEnabled ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"CursorMenu", L"Control", mouseMode ? L"Mouse" : L"Keyboard", path.c_str());
    Bridge::SetMouse(cursorEnabled && mouseMode);
}

static void saveMod(const Mod& mod) {
    WritePrivateProfileStringW(L"Mods", mod.id.c_str(), mod.enabled ? L"1" : L"0",
                               (root / L"settings.ini").c_str());
}

static void drawText(float x, float y, float size, uint32_t color, const char* text) {
    if (drawingModFrame && text)
        ImGui::GetForegroundDrawList()->AddText(font, size, {x, y}, color, text);
}

static NeighborHostV1 host{sizeof(host), 1, Log, drawText};
struct GameTask {
    Mod* mod;
    NeighborGameTask callback;
    void* data;
};
static std::deque<GameTask> gameTasks;
static std::atomic<bool> haveGameTasks = false;
static thread_local Mod* callbackOwner = nullptr;

struct ModCallbackScope {
    Mod* previous;
    explicit ModCallbackScope(Mod& mod) : previous(std::exchange(callbackOwner, &mod)) {}
    ~ModCallbackScope() { callbackOwner = previous; }
};

static void logMod(void* context, const char* message) {
    if (!message) return;
    const auto& mod = *static_cast<Mod*>(context);
    Log(("[" + utf8(mod.id) + "] " + message).c_str());
}

static int getSetting(void* context, const char* key, int32_t* value) {
    if (context != callbackOwner || !context || !key || !value) return 0;
    const auto& mod = *static_cast<Mod*>(context);
    const auto wideKey = wide(key);
    for (const auto& field : mod.fields) {
        if (_wcsicmp(field.key.c_str(), wideKey.c_str()) == 0) {
            *value = field.value;
            return 1;
        }
    }
    return 0;
}

static int queueGameTask(void* context, NeighborGameTask task, void* data) {
    if (!context || context != callbackOwner || !task || !Bridge::HasGameThread()) return 0;
    auto& mod = *static_cast<Mod*>(context);
    if (!mod.active || mod.gameTaskFailed || gameTasks.size() >= 64) return 0;
    gameTasks.push_back({&mod, task, data});
    haveGameTasks = true;
    return 1;
}

static bool runGameTask(const GameTask& task) {
    __try {
        task.callback(task.data);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void RunModGameTasks() {
    if (!haveGameTasks) return;
    std::unique_lock<std::recursive_mutex> guard(uiLock, std::try_to_lock);
    if (!guard.owns_lock() || callbackOwner) return;
    const auto count = std::min<size_t>(gameTasks.size(), 4);
    for (size_t i = 0; i < count; ++i) {
        const auto task = gameTasks.front();
        gameTasks.pop_front();
        if (!task.mod->active || task.mod->gameTaskFailed) continue;
        ModCallbackScope scope(*task.mod);
        if (!runGameTask(task)) task.mod->gameTaskFailed = true;
    }
    haveGameTasks = !gameTasks.empty();
}

static bool resolveApi(Mod& mod, FARPROC v2, FARPROC v1) {
    __try {
        if (v2) {
            const auto* api = reinterpret_cast<NeighborModEntryV2>(v2)(2);
            if (api && api->size >= sizeof(mod.api2) && api->abi == 2) {
                mod.api2 = *api;
                return true;
            }
        }
        if (v1) {
            const auto* api = reinterpret_cast<NeighborModEntry>(v1)(1);
            if (api && api->size >= sizeof(mod.api1) && api->abi == 1) {
                mod.api1 = *api;
                return true;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

static bool safeEnable(Mod& mod) {
    __try {
        if (mod.api2.abi) return !mod.api2.OnEnable || mod.api2.OnEnable(&mod.host2) != 0;
        return !mod.api1.OnEnable || mod.api1.OnEnable(&host) != 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool safeDisable(Mod& mod) {
    __try {
        auto disable = mod.api2.abi ? mod.api2.OnDisable : mod.api1.OnDisable;
        if (disable) disable();
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool safeFrame(Mod& mod, const NeighborFrameV1* frame) {
    __try {
        auto callback = mod.api2.abi ? mod.api2.OnFrame : mod.api1.OnFrame;
        if (callback) callback(frame);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool safeContext(Mod& mod, const NeighborContextV2* context) {
    __try {
        mod.api2.OnContextChanged(context);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool safeSettings(Mod& mod) {
    __try {
        mod.api2.OnSettingsChanged();
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void failure(Mod& mod, L10n::Key error, std::string detail = {}) {
    mod.error = error;
    mod.detail = std::move(detail);
    logMod(&mod, (std::string(L10n::Text(error)) + mod.detail).c_str());
}

static Mod* findMod(const std::wstring& id) {
    for (auto& mod : mods)
        if (_wcsicmp(mod.id.c_str(), id.c_str()) == 0) return &mod;
    return nullptr;
}

static bool hasId(const std::vector<std::wstring>& ids, const std::wstring& id) {
    return std::any_of(ids.begin(), ids.end(), [&](const auto& item) {
        return _wcsicmp(item.c_str(), id.c_str()) == 0;
    });
}

static void disableMod(Mod& mod, L10n::Key error = L10n::Key::None) {
    if (!mod.active) return;
    mod.active = mod.enabled = false;
    gameTasks.erase(std::remove_if(gameTasks.begin(), gameTasks.end(),
        [&](const GameTask& task) { return task.mod == &mod; }), gameTasks.end());
    haveGameTasks = !gameTasks.empty();
    for (auto& dependent : mods) {
        if (dependent.active && hasId(dependent.requires, mod.id))
            disableMod(dependent, L10n::Key::Dependency);
    }
    if (mod.builtin) {
        cursorEnabled = false;
        saveCursor();
    } else {
        ModCallbackScope scope(mod);
        if (!safeDisable(mod)) failure(mod, L10n::Key::DisableFailed);
        if (error != L10n::Key::None) failure(mod, error);
        saveMod(mod);
    }
}

static bool enable(Mod& mod, bool value) {
    if (!value) {
        for (const auto& dependent : mods) {
            if (dependent.active && hasId(dependent.requires, mod.id)) {
                failure(mod, L10n::Key::RequiredBy, utf8(dependent.id));
                return false;
            }
        }
        mod.error = L10n::Key::None;
        disableMod(mod);
        return true;
    }
    if (mod.active) return true;

    mod.error = L10n::Key::None;
    mod.detail.clear();
    for (const auto& other : mods) {
        if (other.active && (hasId(mod.conflicts, other.id) || hasId(other.conflicts, mod.id))) {
            failure(mod, L10n::Key::Conflict, utf8(other.id));
            return false;
        }
    }
    if (mod.builtin) {
        cursorEnabled = mod.enabled = mod.active = true;
        saveCursor();
        return true;
    }
    if (safeMode) { failure(mod, L10n::Key::SafeMode); return false; }
    for (const auto& id : mod.requires) {
        auto* dependency = findMod(id);
        if (!dependency || !dependency->active) {
            failure(mod, L10n::Key::Dependency, utf8(id));
            return false;
        }
    }
    if (!mod.dll) {
        const auto path = mod.dir / L"mod.dll";
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) { failure(mod, L10n::Key::MissingDll); return false; }
        mod.dll = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!mod.dll) { failure(mod, L10n::Key::LoadFailed, std::to_string(GetLastError())); return false; }
        auto v2 = GetProcAddress(mod.dll, "NeighborMod_GetAPIv2");
        auto v1 = GetProcAddress(mod.dll, "NeighborMod_GetAPI");
        if (!resolveApi(mod, v2, v1)) {
            failure(mod, v1 || v2 ? L10n::Key::InvalidApi : L10n::Key::UnsupportedAbi);
            return false;
        }
        mod.host2 = {sizeof(NeighborHostV2), 2, &mod, logMod, drawText, getSetting, queueGameTask};
    }
    ModCallbackScope scope(mod);
    if ((!mod.api1.abi && !mod.api2.abi) || !safeEnable(mod)) {
        failure(mod, L10n::Key::EnableFailed);
        return false;
    }
    mod.contextBits = UINT32_MAX;
    mod.gameTaskFailed = false;
    mod.active = mod.enabled = true;
    saveMod(mod);
    logMod(&mod, "Enabled");
    return true;
}

static void startMod(Mod& mod) {
    if (!mod.autoEnable || mod.startState == 2) return;
    if (mod.startState == 1) {
        failure(mod, L10n::Key::Cycle);
        return;
    }
    mod.startState = 1;
    for (const auto& id : mod.requires) {
        if (auto* dependency = findMod(id)) startMod(*dependency);
    }
    if (mod.error != L10n::Key::Cycle) enable(mod, true);
    mod.startState = 2;
}

static void changeSetting(Mod& mod, Manifest::Setting& field, int value) {
    if (field.value == value) return;
    const auto serialized = std::to_wstring(value);
    if (!WritePrivateProfileStringW(mod.id.c_str(), field.key.c_str(), serialized.c_str(),
                                    (root / L"mod-settings.ini").c_str())) {
        failure(mod, L10n::Key::SaveFailed);
        return;
    }
    field.value = value;
    if (mod.error == L10n::Key::SaveFailed) mod.error = L10n::Key::None;
    if (mod.active && mod.api2.OnSettingsChanged) {
        ModCallbackScope scope(mod);
        if (!safeSettings(mod)) disableMod(mod, L10n::Key::CallbackFailed);
    }
}

static void loadMods() {
    const auto cfg = root / L"settings.ini";
    cursorEnabled = ini(cfg, L"CursorMenu", L"Enabled", L"1") == L"1";
    mouseMode = ini(cfg, L"CursorMenu", L"Control", L"Mouse") != L"Keyboard";
    safeMode = ini(cfg, L"Loader", L"SafeMode", L"0") == L"1";
    Mod builtin;
    builtin.id = L"CursorMenu";
    builtin.name = "CursorMenu";
    builtin.version = "0.5.0";
    builtin.author = "Simple Mods";
    builtin.enabled = builtin.active = cursorEnabled;
    builtin.builtin = true;
    mods.push_back(std::move(builtin));

    std::error_code ec;
    fs::create_directories(root / L"mods", ec);
    fs::directory_iterator entry(root / L"mods", ec), end;
    for (; !ec && entry != end; entry.increment(ec)) {
        std::error_code fileError;
        if (!entry->is_directory(fileError)) continue;
        const auto manifest = entry->path() / L"mod.ini";
        if (!fs::is_regular_file(manifest, fileError)) continue;
        Mod mod;
        mod.id = entry->path().filename().wstring();
        if (findMod(mod.id)) continue;
        mod.dir = entry->path();
        mod.name = utf8(ini(manifest, L"Mod", L"Name", mod.id.c_str()));
        mod.nameRu = utf8(ini(manifest, L"Mod", L"Name.ru"));
        mod.description = utf8(ini(manifest, L"Mod", L"Description"));
        mod.descriptionRu = utf8(ini(manifest, L"Mod", L"Description.ru"));
        mod.version = utf8(ini(manifest, L"Mod", L"Version", L"1.0"));
        mod.author = utf8(ini(manifest, L"Mod", L"Author"));
        mod.requires = Manifest::Split(ini(manifest, L"Mod", L"Requires"), L',');
        mod.conflicts = Manifest::Split(ini(manifest, L"Mod", L"Conflicts"), L',');
        mod.fields = Manifest::ReadSettings(manifest, root / L"mod-settings.ini", mod.id, mod.manifestWarning);
        mod.autoEnable = ini(cfg, L"Mods", mod.id.c_str(), L"0") == L"1";
        mods.push_back(std::move(mod));
    }
    if (ec) Log("Could not scan the mods folder.");
    std::sort(mods.begin() + 1, mods.end(), [](const Mod& a, const Mod& b) {
        return _wcsicmp(a.id.c_str(), b.id.c_str()) < 0;
    });
    Bridge::SetMouse(cursorEnabled && mouseMode);
}
