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
consteval int WINDOW_HEIGHT = 250;
consteval int WINDOW_WIDTH = 700;
consteval int EDIT_WIDTH = 500;
consteval int GAP_HEIGHT = 25;
consteval int GAP_EDIT_BUTTON = 10;
consteval int BUTTON_WIDTH = 100;
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
	BOOL stillXORing;
	HANDLE threadHandle;
};

//FILEPROVIDERs IDs
constexpr int ID_MENU_SRC = 100;
constexpr int ID_MENU_DST = 101;
constexpr int ID_MENU_KEY = 102;

class FILEPROVIDER {
private:

	static std::map<HMENU, HWND> buttonSearchFile;
	static std::map<HMENU, HWND> buttonSaveFile;

	static void createField(std::wstring label, HMENU buttonAction, POINT startLocation, HWND parentWindow, std::map<HMENU, HWND>& dest) {
		const HINSTANCE appInstance = (HINSTANCE)GetWindowLongPtr(parentWindow, GWLP_HINSTANCE);
		const auto LABEL_WIDTH = label.size() * 25;

		CreateWindow(L"STATIC", label.c_str(), WS_VISIBLE | WS_CHILD,
			startLocation.x, startLocation.y, LABEL_WIDTH, GAP_HEIGHT, parentWindow, NULL, appInstance, NULL);

		CreateWindow(L"BUTTON", L"Search", WS_VISIBLE | WS_CHILD,
			startLocation.x + EDIT_WIDTH + GAP_EDIT_BUTTON, startLocation.y + GAP_HEIGHT, BUTTON_WIDTH, GAP_HEIGHT, parentWindow, buttonAction, appInstance, NULL);

		// No error catching code because yes
		dest.emplace(buttonAction, CreateWindow(L"EDIT", NULL, WS_VISIBLE | WS_CHILD,
			startLocation.x, startLocation.y + GAP_HEIGHT, EDIT_WIDTH, GAP_HEIGHT, parentWindow, NULL, appInstance, NULL));
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
		CW_USEDEFAULT,
		CW_USEDEFAULT,
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
		dst = CreateFile(fPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

LRESULT CALLBACK mainWindowProcedure(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	LOGFONT lf;
	WORD wmID, wmEvent;

	switch (Msg) {
	case WM_ENABLE:
		if (wParam != TRUE)
			break;
		
		auto userPtr = (LPVOID)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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

			EncryptionWindowState* encExt = (EncryptionWindowState*)VirtualAlloc(NULL, sizeof(EncryptionWindowState), MEM_COMMIT, NULL); // watch out for memory leak
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

			auto secondWindow = CreateWindowEx(NULL, L"Encryption Window", L"Encryption Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
				300, 100, hwnd, NULL, progressWindowClass.hInstance, &encExt);

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

		CreateWindow(L"BUTTON", L"Encrypt", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			10, GAP_HEIGHT * 7, BUTTON_WIDTH, GAP_HEIGHT, hwnd, (HMENU)103, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)NULL);
		break;
	}
	return DefWindowProc(hwnd, Msg, wParam, lParam);
}

constexpr auto PB_WIDTH = 200;
constexpr auto PB_HEIGHT = 200;

consteval int ENC_WINDOW_HEIGHT = 200;
consteval int ENC_WINDOW_WIDTH = 400;
constexpr int xoringBufferLength = 512;
consteval int ENC_WINDOW_MAX_DIGITS = 10;

DWORD startXORing(EncryptionWindowState* appState) {

	auto encryptionData = &appState->threadEncryptionData;

	auto acquireFileSize = [](const HANDLE& fHandle) -> size_t {
		BY_HANDLE_FILE_INFORMATION fileData{};
		GetFileInformationByHandle(fHandle, &fileData);
		return fileData.nFileSizeLow | ((size_t)fileData.nFileSizeHigh) << 32;
	};

	char xorKey{};
	char buff[xoringBufferLength]{};

	auto srcFileSize = acquireFileSize(encryptionData->handles.srcFileHandle);
	auto keySize = acquireFileSize(encryptionData->handles.keyFileHandle);

	wchar_t currentByteLabel[16]{};
	wchar_t maxBytesLabel[16]{};
	wchar_t keySizeLabel[16]{};

	wsprintf(currentByteLabel, L"%u", (size_t)0);
	wsprintf(maxBytesLabel, L"%zu", srcFileSize);
	wsprintf(keySizeLabel, L"%zu", keySize);


	SetWindowText(encryptionData->currentByte, );
	while(appState->stillXORing){
		ReadFile(srcFile, buffer, xoringBufferLength, NULL, NULL);
	}

	return ERROR_SUCCESS;
}

LRESULT CALLBACK encryptWindowProcedure(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	HWND currentBytesLbl{};
	HWND maxBytesLbl{};
	HWND keyLbl{};

	switch (Msg) {
	case WM_DESTROY:
		auto extraInfo = (EncryptionWindowState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		extraInfo->stillXORing = FALSE;
		if (!WaitForSingleObject(extraInfo->threadHandle, 5000))
			ExitProcess(-1); // Exit with a bang because we do not know if thread is still holding HANDLES, consider it UB

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

		auto extraInfo = (EncryptionWindowState*)(((LPCREATESTRUCT)lParam)->lpCreateParams);
		extraInfo->threadEncryptionData.progressBar = CreateWindow(PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD, 10, 50, 260, 25, hwnd, NULL, currInstance, NULL);

		currentBytesLbl = CreateWindow(L"STATIC", currentBytesDescr.c_str(), WS_VISIBLE | WS_CHILD, 10, 10, currentBytesDescr.size() * 10, 10, hwnd, NULL, currInstance, NULL);
		extraInfo->threadEncryptionData.currentByte = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 10 + currentBytesDescr.size() * 10 + 10, 10, ENC_WINDOW_MAX_DIGITS * 10, 10, hwnd, NULL, currInstance, NULL);

		maxBytesLbl = CreateWindow(L"STATIC", maxBytesDescr.c_str(), WS_VISIBLE | WS_CHILD, 10, 20, maxBytesDescr.size() * 10, 10, hwnd, NULL, currInstance, NULL);
		extraInfo->threadEncryptionData.maxBytes = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 10 + maxBytesDescr.size() * 10 + 10, 20, ENC_WINDOW_MAX_DIGITS * 10, 10, hwnd, NULL, currInstance, NULL);

		keyLbl = CreateWindow(L"STATIC", keyBytesDescr.c_str(), WS_VISIBLE | WS_CHILD, 10, 30, keyBytesDescr.size() * 10, 10, hwnd, NULL, currInstance, NULL);
		extraInfo->threadEncryptionData.keySize = CreateWindow(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 10 + keyBytesDescr.size() * 10 + 10 , 30, ENC_WINDOW_MAX_DIGITS * 10, 10, hwnd, NULL, currInstance, NULL);

		extraInfo->threadHandle = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&startXORing, &extraInfo, NULL, NULL);

		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)extraInfo);
		break;
	}
	return DefWindowProc(hwnd, Msg, wParam, lParam);
}