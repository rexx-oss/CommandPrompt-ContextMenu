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

class CmdCommand : public IExplorerCommand, public IObjectWithSelection, public IObjectWithSite
{
    LONG m_cRef = 1;
    IShellItemArray* m_pSelection = nullptr;
    IUnknown* m_pSite = nullptr;

public:
    ~CmdCommand() {
        if (m_pSelection) { m_pSelection->Release(); m_pSelection = nullptr; }
        if (m_pSite) { m_pSite->Release(); m_pSite = nullptr; }
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        static const QITAB qit[] = {
            QITABENT(CmdCommand, IExplorerCommand),
            QITABENT(CmdCommand, IObjectWithSelection),
            QITABENT(CmdCommand, IObjectWithSite),
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

    // IObjectWithSite (Used to identify the folder when right-clicking blank space)
    IFACEMETHODIMP SetSite(IUnknown* pUnkSite) override {
        if (m_pSite) { m_pSite->Release(); m_pSite = nullptr; }
        if (pUnkSite) {
            m_pSite = pUnkSite;
            m_pSite->AddRef();
        }
        return S_OK;
    }
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        return m_pSite ? m_pSite->QueryInterface(riid, ppv) : E_NOINTERFACE;
    }

    // IObjectWithSelection
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

    // Helper: Gets the folder path when right-clicking background
    std::wstring GetCurrentFolderFromSite() {
        if (!m_pSite) return L"";

        IServiceProvider* pProvider = nullptr;
        if (SUCCEEDED(m_pSite->QueryInterface(IID_PPV_ARGS(&pProvider)))) {
            IShellBrowser* pBrowser = nullptr;
            if (SUCCEEDED(pProvider->QueryService(SID_SShellBrowser, IID_PPV_ARGS(&pBrowser)))) {
                IShellView* pView = nullptr;
                if (SUCCEEDED(pBrowser->QueryActiveShellView(&pView))) {
                    IFolderView* pFolderView = nullptr;
                    if (SUCCEEDED(pView->QueryInterface(IID_PPV_ARGS(&pFolderView)))) {
                        IPersistFolder2* pPersist = nullptr;
                        if (SUCCEEDED(pFolderView->GetFolder(IID_PPV_ARGS(&pPersist)))) {
                            PIDLIST_ABSOLUTE pidl = nullptr;
                            if (SUCCEEDED(pPersist->GetCurFolder(&pidl)) && pidl) {
                                WCHAR path[MAX_PATH] = { 0 };
                                if (SHGetPathFromIDListW(pidl, path)) {
                                    CoTaskMemFree(pidl);
                                    pPersist->Release();
                                    pFolderView->Release();
                                    pView->Release();
                                    pBrowser->Release();
                                    pProvider->Release();
                                    return path;
                                }
                                CoTaskMemFree(pidl);
                            }
                            pPersist->Release();
                        }
                        pFolderView->Release();
                    }
                    pView->Release();
                }
                pBrowser->Release();
            }
            pProvider->Release();
        }
        return L"";
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

        IShellItemArray* items = psiItemArray ? psiItemArray : m_pSelection;
        DWORD count = 0;
        if (items) items->GetCount(&count);

        // CASE 1: Right-clicking EMPTY BACKGROUND (Folder background or Desktop)
        if (count == 0) {
            std::wstring folder = GetCurrentFolderFromSite();
            // If the background folder is a virtual location (like "This PC"), it has no DOS path -> Hide
            if (folder.empty() || !PathFileExistsW(folder.c_str())) {
                *pCmdState = ECS_HIDDEN;
                return S_OK;
            }
            *pCmdState = ECS_ENABLED;
            return S_OK;
        }

        // CASE 2: Right-clicking AN ITEM (File, Folder, or Drive)
        IShellItem* item = nullptr;
        if (SUCCEEDED(items->GetItemAt(0, &item))) {
            SFGAOF attributes = 0;
            // Reject virtual items (e.g. This PC, Network, Recycle Bin icons)
            if (FAILED(item->GetAttributes(SFGAO_FILESYSTEM, &attributes)) || !(attributes & SFGAO_FILESYSTEM)) {
                item->Release();
                *pCmdState = ECS_HIDDEN;
                return S_OK;
            }

            PWSTR rawPath = nullptr;
            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath))) {
                item->Release();
                *pCmdState = ECS_HIDDEN;
                return S_OK;
            }

            std::wstring path = rawPath;
            CoTaskMemFree(rawPath);
            item->Release();

            if (path.empty() || !PathFileExistsW(path.c_str())) {
                *pCmdState = ECS_HIDDEN;
                return S_OK;
            }
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
        DWORD count = 0;
        if (items) items->GetCount(&count);

        std::wstring targetDir;

        // If invoked on empty background
        if (count == 0) {
            targetDir = GetCurrentFolderFromSite();
        } else {
            IShellItem* item = nullptr;
            if (SUCCEEDED(items->GetItemAt(0, &item))) {
                PWSTR rawPath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath))) {
                    targetDir = rawPath;
                    CoTaskMemFree(rawPath);
                }
                item->Release();
            }

            // If a file was selected, open in its parent directory
            if (!targetDir.empty() && !PathIsDirectoryW(targetDir.c_str())) {
                WCHAR parentDir[MAX_PATH] = { 0 };
                wcscpy_s(parentDir, targetDir.c_str());
                PathRemoveFileSpecW(parentDir);
                targetDir = parentDir;
            }
        }

        if (targetDir.empty() || !PathFileExistsW(targetDir.c_str())) {
            return S_OK;
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
