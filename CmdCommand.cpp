#include <windows.h>
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// CLSID: {D3A8E4F1-6B22-4D90-9E11-5A8E893C4177}
static const CLSID CLSID_CmdCommand = 
    { 0xd3a8e4f1, 0x6b22, 0x4d90, { 0x9e, 0x11, 0x5a, 0x8e, 0x89, 0x3c, 0x41, 0x77 } };

// Validates that the target is a genuine physical filesystem folder/file
bool IsValidFilesystemLocation(IShellItemArray* psiItemArray) {
    if (!psiItemArray) return false;
    DWORD count = 0;
    if (FAILED(psiItemArray->GetCount(&count)) || count == 0) return false;

    IShellItem* item = nullptr;
    if (FAILED(psiItemArray->GetItemAt(0, &item))) return false;

    // Reject virtual locations (This PC, Recycle Bin, Network, Control Panel, Phones/MTP)
    SFGAOF attributes = 0;
    if (FAILED(item->GetAttributes(SFGAO_FILESYSTEM, &attributes)) || !(attributes & SFGAO_FILESYSTEM)) {
        item->Release();
        return false;
    }

    PWSTR rawPath = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath))) {
        item->Release();
        return false;
    }

    std::wstring path = rawPath;
    CoTaskMemFree(rawPath);
    item->Release();

    if (path.empty()) return false;

    // Check if it's an existing directory or drive (C:\, D:\, folders)
    if (PathIsDirectoryW(path.c_str())) {
        return PathFileExistsW(path.c_str()) != FALSE;
    }

    // If it's a file, ensure its parent directory exists
    WCHAR parentDir[MAX_PATH] = { 0 };
    wcscpy_s(parentDir, path.c_str());
    PathRemoveFileSpecW(parentDir);
    return PathIsDirectoryW(parentDir) != FALSE;
}

class CmdCommand : public IExplorerCommand, public IObjectWithSelection
{
    LONG m_cRef = 1;
    IShellItemArray* m_pSelection = nullptr;

public:
    ~CmdCommand() {
        if (m_pSelection) { m_pSelection->Release(); m_pSelection = nullptr; }
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        static const QITAB qit[] = {
            QITABENT(CmdCommand, IExplorerCommand),
            QITABENT(CmdCommand, IObjectWithSelection),
            { 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release() override {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    IFACEMETHODIMP SetSelection(IShellItemArray* psia) override {
        if (m_pSelection) { m_pSelection->Release(); m_pSelection = nullptr; }
        if (psia) {
            m_pSelection = psia;
            m_pSelection->AddRef();
        }
        return S_OK;
    }
    IFACEMETHODIMP GetSelection(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        return m_pSelection ? m_pSelection->QueryInterface(riid, ppv) : E_NOINTERFACE;
    }

    IFACEMETHODIMP GetTitle(IShellItemArray*, PWSTR* ppszName) override {
        return SHStrDupW(L"Command Prompt", ppszName);
    }
    IFACEMETHODIMP GetIcon(IShellItemArray*, PWSTR* ppszIcon) override {
        return SHStrDupW(L"C:\\Windows\\System32\\cmd.exe,0", ppszIcon);
    }
    IFACEMETHODIMP GetToolTip(IShellItemArray*, PWSTR* ppszInfo) override {
        if (ppszInfo) *ppszInfo = nullptr;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetCanonicalName(GUID* pguid) override {
        if (!pguid) return E_POINTER;
        *pguid = CLSID_CmdCommand;
        return S_OK;
    }

    // Dynamic visibility check
    IFACEMETHODIMP GetState(IShellItemArray* psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState) override {
        if (!pCmdState) return E_POINTER;

        // Hide from classic context menu
        if (!fOkToBeSlow) {
            *pCmdState = ECS_HIDDEN;
            return S_OK;
        }

        // Hide from virtual folders (This PC, Recycle Bin, Network, Libraries, etc.)
        IShellItemArray* items = psiItemArray ? psiItemArray : m_pSelection;
        if (!IsValidFilesystemLocation(items)) {
            *pCmdState = ECS_HIDDEN;
            return S_OK;
        }

        *pCmdState = ECS_ENABLED;
        return S_OK;
    }

    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override {
        if (!pFlags) return E_POINTER;
        *pFlags = ECF_DEFAULT;
        return S_OK;
    }
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override {
        if (ppEnum) *ppEnum = nullptr;
        return E_NOTIMPL;
    }

    IFACEMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx*) override {
        IShellItemArray* items = psiItemArray ? psiItemArray : m_pSelection;
        if (!items) return S_OK;

        DWORD count = 0;
        items->GetCount(&count);
        if (count == 0) return S_OK;

        IShellItem* item = nullptr;
        if (FAILED(items->GetItemAt(0, &item))) return S_OK;

        PWSTR rawPath = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath))) {
            item->Release();
            return S_OK;
        }

        std::wstring targetDir = rawPath;
        CoTaskMemFree(rawPath);
        item->Release();

        // If a file was right-clicked, open cmd in its parent directory
        if (!PathIsDirectoryW(targetDir.c_str())) {
            WCHAR parentDir[MAX_PATH] = { 0 };
            wcscpy_s(parentDir, targetDir.c_str());
            PathRemoveFileSpecW(parentDir);
            targetDir = parentDir;
        }

        ShellExecuteW(nullptr, L"open", L"C:\\Windows\\System32\\cmd.exe", nullptr, targetDir.c_str(), SW_SHOWNORMAL);
        return S_OK;
    }
};

class ClassFactory : public IClassFactory {
    LONG m_cRef = 1;
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release() override {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;

        CmdCommand* pCmd = new CmdCommand();
        HRESULT hr = pCmd->QueryInterface(riid, ppv);
        pCmd->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL) override { return S_OK; }
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (rclsid == CLSID_CmdCommand) {
        ClassFactory* pFactory = new ClassFactory();
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() { return S_OK; }
