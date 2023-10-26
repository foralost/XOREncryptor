#include <Windows.h>
#include <shobjidl.h> 
#include <string>
#include <map>
#include <functional>
#include <type_traits>
#pragma comment(lib, "comctl32")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
// Win32API name style

// GUI Related consts
constexpr int WINDOW_HEIGHT = 250;
constexpr int WINDOW_WIDTH = 700;
constexpr int EDIT_WIDTH = 500;
constexpr int GAP_HEIGHT = 25;
constexpr int GAP_EDIT_BUTTON = 10;
constexpr int BUTTON_WIDTH = 100;
// Paths related consts
constexpr int MAX_PATH_LEN = 256;
struct EncryptionExtra {
	wchar_t srcFile[MAX_PATH_LEN]{};
	wchar_t dstFile[MAX_PATH_LEN]{};
	wchar_t keyFile[MAX_PATH_LEN]{};

	HANDLE srcFileHandle{ INVALID_HANDLE_VALUE };
	HANDLE dstFileHandle{ INVALID_HANDLE_VALUE };
	HANDLE keyFileHandle{ INVALID_HANDLE_VALUE };
};

struct EncryptionWindowThread {
	EncryptionExtra handles;
	
	HWND progressBar;
	HWND currentByte;
	HWND maxBytes;
	HWND keySize;
};


struct EncryptionWindowState {
	EncryptionWindowThread threadEncryptionData;
	HWND encryptionWindowHandle;;
	BOOL stillXORing;
	HANDLE threadHandle;
};

//FILEPROVIDERs IDs
constexpr int ID_MENU_SRC = 100;
constexpr int ID_MENU_DST = 101;
constexpr int ID_MENU_KEY = 102;
constinit HFONT hDlgFont{};

class FILEPROVIDER {
private:

	static std::map<HMENU, HWND> buttonSearchFile;
	static std::map<HMENU, HWND> buttonSaveFile;

	static void createField(std::wstring label, HMENU buttonAction, POINT startLocation, HWND parentWindow, std::map<HMENU, HWND>& dest) {
		const HINSTANCE appInstance = (HINSTANCE)GetWindowLongPtr(parentWindow, GWLP_HINSTANCE);
		const auto LABEL_WIDTH = label.size() * 25;

		const auto lblDescription = CreateWindow(L"STATIC", label.c_str(), WS_VISIBLE | WS_CHILD,
			startLocation.x, startLocation.y, LABEL_WIDTH, GAP_HEIGHT, parentWindow, NULL, appInstance, NULL);

		const auto btnSearch = CreateWindow(L"BUTTON", L"Search", WS_VISIBLE | WS_CHILD,
			startLocation.x + EDIT_WIDTH + GAP_EDIT_BUTTON, startLocation.y + GAP_HEIGHT, BUTTON_WIDTH, GAP_HEIGHT, parentWindow, buttonAction, appInstance, NULL);
		
		const auto editPath = CreateWindow(L"EDIT", NULL, WS_VISIBLE | WS_CHILD,
			startLocation.x, startLocation.y + GAP_HEIGHT, EDIT_WIDTH, GAP_HEIGHT, parentWindow, NULL, appInstance, NULL);
		
		SendMessage(lblDescription, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));
		SendMessage(btnSearch, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));
		SendMessage(editPath, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));

		// No error catching code because yes
		dest.emplace(buttonAction, editPath);
	}

public:
	static void createSearchField(std::wstring label, HMENU buttonAction, POINT startLocation, HWND parentWindow) {
		createField(label, buttonAction, startLocation, parentWindow, buttonSearchFile);
	}

	static void createSaveField(std::wstring label, HMENU buttonAction, POINT startLocation, HWND parentWindow) {
		createField(label, buttonAction, startLocation, parentWindow, buttonSaveFile);
	}


	static HRESULT handleDialogFile(HWND owner, wchar_t(&dst)[256], CLSID CLSID_FileDialog) {
		IFileDialog* openDialog;
		if (auto lastStatus = CoCreateInstance(CLSID_FileDialog, NULL, CLSCTX_INPROC_SERVER, CLSID_FileDialog == CLSID_FileOpenDialog ? IID_IFileOpenDialog : IID_IFileSaveDialog, (void**)&openDialog);  FAILED(lastStatus)) {
			MessageBox(owner, L"Creating instance failed.", L"Error", MB_ICONERROR | MB_OK);
			return lastStatus;
		}

		if (auto lastStatus = openDialog->Show(owner);  FAILED(lastStatus) && lastStatus != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
			MessageBox(owner, L"Showing dialog failed.", L"Error", MB_ICONERROR | MB_OK);
			openDialog->Release();
			return lastStatus;
		}
		else if (lastStatus == HRESULT_FROM_WIN32(ERROR_CANCELLED))
			return lastStatus;

		IShellItem* res{};
		if (auto lastStatus = openDialog->GetResult(&res); FAILED(lastStatus)) {
			MessageBox(owner, L"Getting result from the dialog failed.", L"Error", MB_ICONERROR | MB_OK);
			openDialog->Release();
			return lastStatus;
		}

		wchar_t* buff{};
		if (auto lastStatus = res->GetDisplayName(SIGDN_FILESYSPATH, (LPWSTR*)&buff); FAILED(lastStatus)) {
			MessageBox(owner, L"Getting display name failed.", L"Error", MB_ICONERROR | MB_OK);
			res->Release();
			openDialog->Release();
			return lastStatus;
		}
		else {
			wcscpy_s(dst, buff);

			res->Release();
			openDialog->Release();
			return 0;
		}
	}

	static bool retrieveText(HMENU from, wchar_t(&to)[MAX_PATH_LEN]) {
		auto findLocation = buttonSaveFile.find((HMENU)from);
		if (findLocation == buttonSaveFile.end()) {
			findLocation = buttonSearchFile.find((HMENU)from);

			if (findLocation == buttonSearchFile.end())
				return false;
		}


		GetWindowTextW(findLocation->second, (LPWSTR)to, MAX_PATH_LEN);
		return true;
	}
	static bool parseButtonLogic(WORD id) {
		wchar_t textBuff[256]{};
		auto findLocation = buttonSaveFile.find((HMENU)id);
		if (findLocation != buttonSaveFile.end()) {
			handleDialogFile((HWND)GetWindowLongPtr(findLocation->second, GWLP_HWNDPARENT), textBuff, CLSID_FileSaveDialog);
		}
		else {
			findLocation = buttonSearchFile.find((HMENU)id);
			if (findLocation == buttonSearchFile.end())
				return false;

			handleDialogFile((HWND)GetWindowLongPtr(findLocation->second, GWLP_HWNDPARENT), textBuff, CLSID_FileOpenDialog);
		}


		SetWindowTextW(findLocation->second, textBuff);
		return true;
	}
};

std::map<HMENU, HWND> FILEPROVIDER::buttonSaveFile = {};
std::map<HMENU, HWND> FILEPROVIDER::buttonSearchFile = {};

LRESULT CALLBACK mainWindowProcedure(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK encryptWindowProcedure(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);

const auto userMainScreenWidth = GetSystemMetrics(SM_CXSCREEN);
const auto userMainScreenHeight = GetSystemMetrics(SM_CYSCREEN);

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
) {
	if (auto CoStatus = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); FAILED(CoStatus)) {
		wchar_t buff[128];
		wsprintfW(buff, L"Failed to initialize Co library. Returned status code: 0x%X", CoStatus);
		MessageBoxW(0, buff, L"Error", MB_ICONERROR | MB_ICONERROR);
		return CoStatus;
	}

	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);

	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, sizeof(NONCLIENTMETRICS));
	hDlgFont = CreateFontIndirect(&(ncm.lfMessageFont));

	WNDCLASS wc{};
	wc.lpfnWndProc = mainWindowProcedure;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"Main Window";
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hCursor = (HCURSOR)LoadCursor(hInstance, IDC_ARROW);

	RegisterClass(&wc);

	HWND hwndMainWindow = CreateWindowEx(
		0,
		L"Main Window",
		L"Main Window",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		(userMainScreenWidth - WINDOW_WIDTH) / 2 ,
		(userMainScreenHeight - WINDOW_HEIGHT) / 2,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		NULL, NULL,
		hInstance,
		NULL
	);

	UpdateWindow(hwndMainWindow);

	MSG msg{};
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	CoUninitialize();
	return 0;
}

BOOL openFiles(HWND parent, EncryptionExtra& data) {
	auto tryToOpen = [parent](HANDLE& dst, const wchar_t* fPath) -> bool {
		dst = CreateFile(fPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (dst == INVALID_HANDLE_VALUE)
		{
			wchar_t msg[256];
			wsprintf(msg, L"Could not open file located at \"%s\" for reading.", fPath);
			MessageBox(parent, msg, L"Error", MB_ICONERROR);
			return false;
		}
		return true;
	};

	auto tryToWrite = [parent](HANDLE& dst, const wchar_t* fPath) -> bool {
		dst = CreateFile(fPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (dst == INVALID_HANDLE_VALUE)
		{
			wchar_t msg[256];
			wsprintf(msg, L"Could not open file located at \"%s\" for writing.", fPath);
			MessageBox(parent, msg, L"Error", MB_ICONERROR);
			return false;
		}
		return true;
	};

	if (!tryToOpen(data.keyFileHandle, data.keyFile))
		return FALSE;
	
	if (!tryToOpen(data.srcFileHandle, data.srcFile))
	{
		CloseHandle(data.keyFileHandle);
		data.keyFileHandle = INVALID_HANDLE_VALUE;
		return FALSE;
	}

	if (!tryToWrite(data.dstFileHandle, data.dstFile))
	{
		CloseHandle(data.keyFileHandle);
		CloseHandle(data.srcFileHandle);
		data.keyFileHandle = INVALID_HANDLE_VALUE;
		data.srcFileHandle = INVALID_HANDLE_VALUE;
		return FALSE;
	}

	return TRUE;
}
constexpr auto ENCRYPTION_BOX_WIDTH = 300;
constexpr auto ENCRYPTION_BOX_HEIGHT = 175;

LRESULT CALLBACK mainWindowProcedure(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	LOGFONT lf{};
	WORD wmID{}, wmEvent{};
	LPVOID userPtr{};

	switch (Msg) {
	case WM_ENABLE:
		if (wParam != TRUE)
			break;
		
		userPtr = (LPVOID)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (!userPtr)
			break;

		VirtualFree(userPtr, NULL, MEM_RELEASE);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)NULL);
		break;
	case WM_COMMAND:
		wmID = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		if (FILEPROVIDER::parseButtonLogic(wmID)) break;
		switch (wmID) {
		case 103:
			EncryptionExtra ext;
			FILEPROVIDER::retrieveText((HMENU)ID_MENU_DST, ext.dstFile);
			FILEPROVIDER::retrieveText((HMENU)ID_MENU_SRC, ext.srcFile);
			FILEPROVIDER::retrieveText((HMENU)ID_MENU_KEY, ext.keyFile);

			if (!openFiles(hwnd, ext))
				break;

			EncryptionWindowState* encExt = (EncryptionWindowState*)VirtualAlloc(NULL, sizeof(EncryptionWindowState), MEM_COMMIT, PAGE_READWRITE); // watch out for memory leak
			encExt->stillXORing = true;
			encExt->threadEncryptionData.handles = ext;
			encExt->threadEncryptionData.currentByte = (HWND)INVALID_HANDLE_VALUE;
			encExt->threadEncryptionData.maxBytes = (HWND)INVALID_HANDLE_VALUE;
			encExt->threadEncryptionData.progressBar = (HWND)INVALID_HANDLE_VALUE;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)encExt);

			WNDCLASSW progressWindowClass{};
			progressWindowClass.lpfnWndProc = encryptWindowProcedure;
			progressWindowClass.lpszClassName = L"Encryption Window";
			progressWindowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
			progressWindowClass.hInstance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
			RegisterClass(&progressWindowClass);
			EnableWindow(hwnd, FALSE);

			auto secondWindow = CreateWindowEx(NULL, L"Encryption Window", L"Encryption Window", 
				WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME | WS_VISIBLE, 
				(userMainScreenWidth - ENCRYPTION_BOX_WIDTH) / 2, (userMainScreenHeight - ENCRYPTION_BOX_HEIGHT) / 2,
				ENCRYPTION_BOX_WIDTH, ENCRYPTION_BOX_HEIGHT, hwnd, NULL, progressWindowClass.hInstance, encExt);
			
			break;
		}
		break;

	case WM_DESTROY:
		ExitProcess(0);
		break;

	case WM_CREATE:
		FILEPROVIDER::createSearchField(L"Source file:", (HMENU)ID_MENU_SRC, POINT{ 10,10 }, hwnd);
		FILEPROVIDER::createSaveField(L"Destination file:", (HMENU)ID_MENU_DST, POINT{ 10,60 }, hwnd);
		FILEPROVIDER::createSearchField(L"File with the key:", (HMENU)ID_MENU_KEY, POINT{ 10, 110 }, hwnd);
		const auto btnEncrypt = CreateWindow(L"BUTTON", L"Encrypt", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			10, GAP_HEIGHT * 7, BUTTON_WIDTH, GAP_HEIGHT, hwnd, (HMENU)103, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

		SendMessage(btnEncrypt, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)NULL);
		break;
	}
	return DefWindowProc(hwnd, Msg, wParam, lParam);
}

constexpr auto PB_WIDTH = 200;
constexpr auto PB_HEIGHT = 200;

constexpr int ENC_WINDOW_HEIGHT = 200;
constexpr int ENC_WINDOW_WIDTH = 400;
constexpr int xoringBufferLength = 512;
constexpr int ENC_WINDOW_MAX_DIGITS = 10;

DWORD startXORing(EncryptionWindowState* appState) {

	auto encryptionData = &appState->threadEncryptionData;

	auto acquireFileSize = [](const HANDLE& fHandle) -> DWORD {
		BY_HANDLE_FILE_INFORMATION fileData{};
		GetFileInformationByHandle(fHandle, &fileData);
		return fileData.nFileSizeLow;
		};

	char xorKey[xoringBufferLength]{};
	ReadFile(encryptionData->handles.keyFileHandle, xorKey, xoringBufferLength, NULL, NULL);

	char buff[xoringBufferLength]{};

	uint64_t dstFileSize = 0;
	auto srcFileSize = acquireFileSize(encryptionData->handles.srcFileHandle);
	auto keySize = acquireFileSize(encryptionData->handles.keyFileHandle);

	SendMessage(encryptionData->progressBar, PBM_SETRANGE, 0, srcFileSize);

	wchar_t currentByteLabel[16]{};
	wchar_t maxBytesLabel[16]{};
	wchar_t keySizeLabel[16]{};
	
	DWORD bytesProcessed{};
	
	wsprintf(maxBytesLabel, L"%u", srcFileSize);
	wsprintf(keySizeLabel, L"%u", keySize);

	SetWindowText(encryptionData->currentByte, currentByteLabel);
	SetWindowText(encryptionData->keySize, keySizeLabel);
	SetWindowText(encryptionData->maxBytes, maxBytesLabel);
	SendMessage(encryptionData->progressBar, PBM_SETRANGE, 0, srcFileSize);

	while (appState->stillXORing) {
		DWORD currPackageCount{};
		ReadFile(encryptionData->handles.srcFileHandle, buff, xoringBufferLength, &currPackageCount, NULL);
		
		bytesProcessed += currPackageCount;
		for(uint16_t i = 0; i < currPackageCount; i++)
			buff[i] ^= xorKey[i % keySize];
		
		WriteFile(encryptionData->handles.dstFileHandle, buff, currPackageCount, &currPackageCount, NULL);

		wsprintf(currentByteLabel, L"%u", bytesProcessed);
		SetWindowText(encryptionData->currentByte, currentByteLabel);

		SendMessage(encryptionData->progressBar, PBM_SETPOS, bytesProcessed, 0);
		if (bytesProcessed == srcFileSize)
			appState->stillXORing = false;
	}

	CloseHandle(encryptionData->handles.srcFileHandle);
	CloseHandle(encryptionData->handles.dstFileHandle);
	CloseHandle(encryptionData->handles.keyFileHandle);

	if (bytesProcessed == srcFileSize) {
		MessageBox(appState->encryptionWindowHandle, L"Succesfully encrypted data.", L"Encryption", MB_OK | MB_ICONINFORMATION);
		return ERROR_SUCCESS;
	}
	else {
		MessageBox(appState->encryptionWindowHandle, L"XORing was cancelled.", L"Encryption", MB_OK | MB_ICONWARNING);
		return ERROR_CANCELLED;
	}
}

LRESULT CALLBACK encryptWindowProcedure(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	HWND currentBytesLbl{};
	HWND maxBytesLbl{};
	HWND keyLbl{};
	EncryptionWindowState* extraInfo{};

	switch (Msg) {
	case WM_CLOSE:
		extraInfo = (EncryptionWindowState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		extraInfo->stillXORing = false;
		if (!WaitForSingleObject(extraInfo->threadHandle, 5000) == WAIT_TIMEOUT) {
			MessageBox(hwnd, L"Could not finish the encrypting thread. Closing the application.", L"Error", MB_ICONERROR);
			ExitProcess(-1); // Exit with a bang because we do not know if thread is still holding HANDLES, consider it UB
		}

		EnableWindow((HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT), TRUE);
		SetActiveWindow((HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT));
		DestroyWindow(hwnd);
		break;
	
	case WM_CREATE:
		const std::wstring currentBytesDescr(L"Bytes XORed: ");
		const std::wstring maxBytesDescr(L"Bytes to XOR: ");
		const std::wstring keyBytesDescr(L"Key length: ");
		const HINSTANCE currInstance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

		INITCOMMONCONTROLSEX initCC{ sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
		if (!InitCommonControlsEx(&initCC)) { // We gonna use only progress bar for now.
			MessageBoxW(hwnd, L"Could not initiate the progress bar control.", L"Error", MB_ICONERROR | MB_OK);
			return -1;
		}

		extraInfo = (EncryptionWindowState*)(((LPCREATESTRUCT)lParam)->lpCreateParams);

		extraInfo->threadEncryptionData.progressBar = CreateWindow(PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD, 10, 80, 260, 25, hwnd, NULL, currInstance, NULL);

		currentBytesLbl = CreateWindow(L"STATIC", currentBytesDescr.c_str(), WS_VISIBLE | WS_CHILD, 10, 10, currentBytesDescr.size() * 10, 20, hwnd, NULL, currInstance, NULL);
		extraInfo->threadEncryptionData.currentByte = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 10 + currentBytesDescr.size() * 10 + 10, 10, ENC_WINDOW_MAX_DIGITS * 10, 20, hwnd, NULL, currInstance, NULL);

		maxBytesLbl = CreateWindow(L"STATIC", maxBytesDescr.c_str(), WS_VISIBLE | WS_CHILD, 10, 30, maxBytesDescr.size() * 10, 20, hwnd, NULL, currInstance, NULL);
		extraInfo->threadEncryptionData.maxBytes = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 10 + maxBytesDescr.size() * 10 + 10, 30, ENC_WINDOW_MAX_DIGITS * 10, 20, hwnd, NULL, currInstance, NULL);

		keyLbl = CreateWindow(L"STATIC", keyBytesDescr.c_str(), WS_VISIBLE | WS_CHILD, 10, 50, keyBytesDescr.size() * 10, 20, hwnd, NULL, currInstance, NULL);
		extraInfo->threadEncryptionData.keySize = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 10 + keyBytesDescr.size() * 10 + 10 , 50, ENC_WINDOW_MAX_DIGITS * 10, 20, hwnd, NULL, currInstance, NULL);

		extraInfo->threadHandle = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&startXORing, extraInfo, NULL, NULL);
		extraInfo->encryptionWindowHandle = hwnd;
		
		SendMessage(keyLbl, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));
		SendMessage(maxBytesLbl, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));
		SendMessage(currentBytesLbl, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));

		SendMessage(extraInfo->threadEncryptionData.currentByte, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));
		SendMessage(extraInfo->threadEncryptionData.maxBytes, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));
		SendMessage(extraInfo->threadEncryptionData.keySize, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));

		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)extraInfo);
		break;
	}
	return DefWindowProc(hwnd, Msg, wParam, lParam);
}