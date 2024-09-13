#define DEVCPP
#include <Windows.h>
#include <string>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iostream>
#include <tlhelp32.h>
#include <io.h>
#include <cstdarg>
#include <ctime>
#include <sys/stat.h>
#include <conio.h>
#include <cmath>
#include <map>
#include <vector>
using namespace std;

#define USWND_CLASS_NAME "UserStimulatorTipClass"
#define USWND_CAPTION "UserStimulatorTip"

#define SAMPLE_RECORD_CD 50.0				//ms
#define SMALL_SAMPLE_RECORD_CD 25.0			//ms
#define SMALL_SAMPLE_MIN_DISTANCE 400.0		//px

#define TIP_FS 30
#define TIP_FONTNAME "Minecraft AE Pixel"
#define TIPWND_Y (scr_h - taskbar_h - 100 - tipwnd_h)

#define CJZAPI __stdcall
#define PI 3.14159265358979323846f

bool g_admin {false};
HINSTANCE _hInstance{nullptr};
HINSTANCE _hPrevInstance{nullptr};
LPSTR _lpCmdLine{nullptr};
int _nShowCmd;

#define CFOPER_NONE 0
#define CFOPER_STOP 1
#define CFOPER_WAIT 2
constexpr UINT operation_when_check_failure = CFOPER_WAIT; 

enum UIStage : ULONG
{
	UILoad,
	UIMain,
	UIFileManage,
	UIRecordPre,
	UIRecording,
	UIRecordPause,
	UIRecordEnd,
	UIExecutePre,
	UIExecuting,
	UIExecuteEnd,
	UIDisplaying,
};

UIStage g_ui_stage{UILoad};
HWND hwnd{nullptr};
HWND hwnd_console{nullptr};
int scr_w;
int scr_h;
int taskbar_h;
constexpr int tipwnd_h = 60;

#define KEY_DOWN(sth) (GetAsyncKeyState(sth) & 0x8000 ? 1 : 0)
#define K(sth) (KEY_DOWN(sth) && GetForegroundWindow() == hwnd_console)
#define fequ(f1, f2) (abs((f1)-(f2)) < 0.001)
void ErrMessage(const string& s);

void TipWindowSwitch(bool enable)
{
	if (enable)
	{
		SetWindowPos(hwnd, HWND_TOPMOST, 0, TIPWND_Y, 0, 0, SWP_NOSIZE);
	}else{
		MoveWindow(hwnd, 0, scr_h + 100, scr_w, tipwnd_h, FALSE);
//		MoveWindow(hwnd, 0, 0, scr_w, tipwnd_h, FALSE);
	}
}
inline double Distance(LONG x1, LONG y1, LONG x2, LONG y2)
{
	return sqrt(pow(x1-x2, 2) + pow(y1-y2, 2));
}
template <typename _T>
inline _T CJZAPI ClampA(_T& val, _T min=0, _T max=2147483647) {	//限定范围
	if(val < min) val = min;
	else if(val > max) val = max;
	return val;
}
template <typename _T>
inline _T CJZAPI Clamp(_T val, _T min=0, _T max=2147483647) {	//限定范围
	if(val < min) val = min;
	else if(val > max) val = max;
	return val;
}
template <typename _T>
inline double CJZAPI Lerp(_T startValue, _T endValue, double ratio)
{
	return startValue + (endValue - startValue) * ratio;
}
template <typename _T>
inline double CJZAPI LerpClamp(_T startValue, _T endValue, double ratio)
{
	return Clamp<double>(Lerp(startValue,endValue,ratio), min(startValue,endValue), max(endValue,startValue));
}
inline double CJZAPI EaseInOutSine(double _x)
{	//retval,_x ∈ [0,1]
	return -(cos(PI * _x) - 1) / 2;
}
// 辅助函数：RGB到HSV颜色转换
void RGBtoHSV(COLORREF rgb, double& h, double& s, double& v) {
	double r = GetRValue(rgb) / 255.0;
	double g = GetGValue(rgb) / 255.0;
	double b = GetBValue(rgb) / 255.0;
	
	double minVal = std::min(std::min(r, g), b);
	double maxVal = std::max(std::max(r, g), b);
	double delta = maxVal - minVal;
	
	// 计算色相
	if (delta > 0) {
		if (maxVal == r)
			h = 60.0 * fmod((g - b) / delta, 6.0);
		else if (maxVal == g)
			h = 60.0 * ((b - r) / delta + 2.0);
		else if (maxVal == b)
			h = 60.0 * ((r - g) / delta + 4.0);
	} else {
		h = 0.0;
	}
	
	// 计算饱和度和亮度
	s = (maxVal > 0) ? (delta / maxVal) : 0.0;
	v = maxVal;
}

// 辅助函数：HSV到RGB颜色转换
COLORREF HSVtoRGB(double h, double s, double v) {
	int hi = static_cast<int>(floor(h / 60.0)) % 6;
	double f = h / 60.0 - floor(h / 60.0);
	double p = v * (1.0 - s);
	double q = v * (1.0 - f * s);
	double t = v * (1.0 - (1.0 - f) * s);
	
	switch (hi) {
		case 0: return RGB(static_cast<int>(v * 255), static_cast<int>(t * 255), static_cast<int>(p * 255));
		case 1: return RGB(static_cast<int>(q * 255), static_cast<int>(v * 255), static_cast<int>(p * 255));
		case 2: return RGB(static_cast<int>(p * 255), static_cast<int>(v * 255), static_cast<int>(t * 255));
		case 3: return RGB(static_cast<int>(p * 255), static_cast<int>(q * 255), static_cast<int>(v * 255));
		case 4: return RGB(static_cast<int>(t * 255), static_cast<int>(p * 255), static_cast<int>(v * 255));
		case 5: return RGB(static_cast<int>(v * 255), static_cast<int>(p * 255), static_cast<int>(q * 255));
		default: return RGB(0, 0, 0);  // Shouldn't reach here
	}
}

// 主函数：返回随时间变化的彩虹色
COLORREF RainbowColor() {
	// 假设时间按秒计算，这里使用系统时间或其他适当的时间源
	double timeInSeconds = GetTickCount() / 1000.0;
	
	// 色相按时间变化
	double hue = fmod(timeInSeconds * 30.0, 360.0);  // 假设每秒变化30度
	
	// 饱和度和亮度保持不变
	double saturation = 1.0;
	double value = 1.0;
	
	// 将HSV颜色转换为RGB并返回
	return HSVtoRGB(hue, saturation, value);
}
COLORREF RainbowColorQuick() {
	// 假设时间按秒计算，这里使用系统时间或其他适当的时间源
	double timeInSeconds = GetTickCount() / 1000.0;
	
	// 色相按时间变化
	double hue = fmod(timeInSeconds * 60.0, 360.0);  // 假设每秒变化30度
	
	// 饱和度和亮度保持不变
	double saturation = 1.0;
	double value = 1.0;
	
	// 将HSV颜色转换为RGB并返回
	return HSVtoRGB(hue, saturation, value);
}
COLORREF CJZAPI StepColor(COLORREF startColor, COLORREF endColor, double rate) 
{
	if(fequ(rate,0.0))	return startColor;
	if(fequ(rate,1.0)) return endColor;
	//颜色的渐变
	int r = (GetRValue(endColor) - GetRValue(startColor));
	int g = (GetGValue(endColor) - GetGValue(startColor));
	int b = (GetBValue(endColor) - GetBValue(startColor));
	
	int nSteps = max(abs(r), max(abs(g), abs(b)));
	if (nSteps < 1) nSteps = 1;
	
	// Calculate the step size for each color
	float rStep = r / (float)nSteps;
	float gStep = g / (float)nSteps;
	float bStep = b / (float)nSteps;
	
	// Reset the colors to the starting position
	float fr = GetRValue(startColor);
	float fg = GetGValue(startColor);
	float fb = GetBValue(startColor);
	
	COLORREF color;
	for (int i = 0; i < int(nSteps * rate); i++) {
		fr += rStep;
		fg += gStep;
		fb += bStep;
		color = RGB((int)(fr + 0.5), (int)(fg + 0.5), (int)(fb + 0.5));
		//color 即为重建颜色
	}
	return color;
}//from https://bbs.csdn.net/topics/240006897 , owner: zgl7903
inline COLORREF CJZAPI InvertedColor(COLORREF color)
{
	return RGB(GetBValue(color), GetGValue(color), GetRValue(color));
}
template <typename T>
string CJZAPI str(const T& value)
{
	stringstream ss;
	ss << value;
	string s;
	ss >> s;
	return s;
}
string CJZAPI sprintf2(const char* szFormat, ...)
{
	va_list _list;
	va_start(_list, szFormat);
	char szBuffer[1024] = "\0";
	_vsnprintf(szBuffer, 1024, szFormat, _list);
	va_end(_list);
	return string{szBuffer};
}
template <typename T>
int CJZAPI ToInt(const T& value)
{
	stringstream ss;
	ss << value;
	int i;
	ss >> i;
	return i;
}
inline bool CJZAPI strequ(char *str, const char *obj)
{	//比较是否一样 
	return (strcmp((const char *)str, obj) == 0 ? true : false);
}
inline bool CJZAPI sequ(const string& s1, const string& s2)
{
	return _stricmp(s1.c_str(), s2.c_str()) == 0;
}
string CJZAPI strtail(const string& s, int cnt = 1) {
	//012 len=3
	//abc   s.substr(2,1)
	if (cnt > s.size())
		return s;
	return s.substr(s.size() - cnt, cnt);
}
string CJZAPI strhead(const string& s, int cnt = 1) {
	if (cnt > s.size())
		return s;
	return s.substr(0, cnt);
}
string CJZAPI strxtail(const string& s, int cnt = 1) {
	if (cnt > s.size())
		return "";
	return s.substr(0, s.size() - cnt);
}
string CJZAPI strxhead(const string& s, int cnt = 1) {
	if (cnt > s.size())
		return "";
	return s.substr(cnt, s.size() - cnt);
}
string CJZAPI strip(const string& s)
{
	string res = s;
	while(!res.empty() && (res.at(0) == '\r' || res.at(0) == '\n' || res.at(0) == '\0'))
		res = res.substr(1, res.size() - 1);
	while(!res.empty() && (res.at(res.size()-1) == '\r' || res.at(res.size()-1) == '\n' || res.at(0) == '\0'))
		res = res.substr(0, res.size() - 1);
	return res;
}
bool CJZAPI ExistFile(const string& sfile)
{	//文件或文件夹都可以 
	return !_access(sfile.c_str(), S_OK);//S_OK表示只检查是否存在 
}
string CJZAPI FileSizeString(size_t fsize)	//Bytes
{
	if (fsize < 1024)
		return str(fsize)+" B";
	if (fsize < 10240)
		return sprintf2("%.1f KB", fsize / 1024.0);
	if (fsize < 1024*1024)
		return sprintf2("%.0f KB", ceil(fsize / 1024.0));
	if (fsize < 1024*1024*1024)
		return sprintf2("%.1f MB", fsize / 1024.0 / 1024.0);
	if (fsize < 1024*1024*1024*1024)
		return sprintf2("%.1f GB", fsize / 1024.0 / 1024.0 / 1024.0);
	return sprintf2("%.1f TB", fsize / 1024.0 / 1024.0 / 1024.0 / 1024.0);
}
bool CJZAPI GetFileSize(const string& path, size_t& res)
{
	ifstream file(path, std::ios::binary | std::ios::ate);
	
	if (file.is_open()) {
		// 获取文件指针的位置，即文件大小
		streampos size = file.tellg();
		res = static_cast<size_t>(size);
		return true;
	}
	return false;
}
bool CJZAPI GetFileModifyTimeString(const string& filePath, string& res)
{
	struct _stat64i32 fileStat;
	
	// 获取文件信息
	if (_stat64i32(filePath.c_str(), &fileStat) == 0) {
		// 获取文件的最后修改时间
		time_t modifiedTime = fileStat.st_mtime;
		
		// 将 time_t 转换为本地时间
		struct tm *timeinfo = localtime(&modifiedTime);
		
		// 将本地时间格式化为字符串
		char buffer[80];
		strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", timeinfo);
		res = string{buffer};
		return true;
	}
	return false;
}
BOOL CJZAPI IsFile(const string& lpPath)
{	//是否是文件 
	int res;
#ifndef DEVCPP 
	struct _stat buf;
	res = _stat(lpPath.c_str(), &buf);
#else
	struct _stat64i32 buf;
	res = _stat64i32(lpPath.c_str(), &buf);
#endif
	return (buf.st_mode & _S_IFREG);
}
BOOL CJZAPI IsDir(const string& lpPath)
{	//是否是文件夹
	int res;
#ifndef DEVCPP 
	struct _stat buf;
	res = _stat(lpPath.c_str(), &buf);
#else
	struct _stat64i32 buf;
	res = _stat64i32(lpPath.c_str(), &buf);
#endif
	return (buf.st_mode & _S_IFDIR);
}
string CJZAPI GetFileDirectory(string path)
{	//返回的最后会有反斜线
	if (IsDir(path))
	{
		if (strtail(path) != "\\")
			path += "\\";
		return path;
	}
	if (strtail(path) == "\\")
		path = strxtail(path);
	string ret = "";
	bool suc = false;
	for (int i = path.size() - 1; i >= 0; i--)
	{
		if (path.at(i) == '\\')
		{
			ret = path.substr(0, i + 1);
			suc = true;
			break;
		}
	}
	if (!suc)
		SetLastError(3L);	//还是要return的
	if (strtail(ret) != "\\")
		ret += "\\";
	return ret;
}
vector<string> CJZAPI GetDirFiles(const string& dir, const string& filter = "*.*")
{
	if (dir.empty() || (filter != "" && !ExistFile(dir)))
	{
		return vector<string>();
	}
	_finddata_t fileDir;
	intptr_t lfDir = 0;
	vector<string> files{};
	string dirp = dir + filter;	//它是查找式的
	
	if ((lfDir = _findfirst(dirp.c_str(), &fileDir)) == -1)
	{
		return vector<string>();
	}
	else {
		do {	//遍历目录
			if (!strequ(fileDir.name, ".") && !strequ(fileDir.name, ".."))	//排除这两个狗东西
				files.push_back(string(fileDir.name));
		} while (_findnext(lfDir, &fileDir) == 0);
	}
	_findclose(lfDir);
	return files;
}
void GetDirFilesR_Proc(vector<string>* result, const string& odir /*backslashed*/, const string& childDir, const string& filter)
{
	vector<string> matchedFiles = GetDirFiles(odir + childDir, filter);
	for (auto& f : matchedFiles)
		if (IsFile(odir + childDir + f))
		{
			result->push_back(childDir + f);
		}
	matchedFiles.clear();
	vector<string> all = GetDirFiles(odir + childDir, "*");
	for (auto& ele : all)
		if (IsDir(odir + childDir + ele))
		{
			GetDirFilesR_Proc(result, odir, childDir + ele + "\\", filter);
		}
	
}
vector<string> CJZAPI GetDirFilesR(const string& dir /*backslashed*/, const string& filter = "*.*")
{
	vector<string> result;
	GetDirFilesR_Proc(&result, dir, "", filter);
	return result;
}
vector<string> CJZAPI CutLine(const string& line, char sep = ' ')
{
	vector<string> ret{};
	if (line.empty()) return ret;
	int p1 = 0, p2 = 0;
	for (int i = 0; i < line.size(); i++)
	{
		if (line.at(i) == sep
			|| i == line.size() - 1)
		{
			p2 = i;
			string s = line.substr(p1, p2 - p1 + (i == line.size() - 1 ? 1 : 0));
			ret.push_back(s);
			p1 = i + 1;
		}
	}
	return ret;
}
vector<string> CJZAPI ReadFileLines(const string& filePath)
{	//读取文件每一行 
	vector<string> ret{};
	fstream fin(filePath, ios::in);
	if (fin.fail())
	{
		ErrMessage("无法打开文件"+filePath);
		return ret;
	}
	while (1)
	{
		char s[256]{0};
		
		if (fin.eof())
			break;
		
		fin.getline(s, 256);
		ret.emplace_back(s);
	}
	fin.close();
	return ret;
}

void CJZAPI SetColor(UINT uFore,UINT uBack) 
{
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, uFore + uBack * 0x10);
}
void CJZAPI SetPos0(COORD a) 
{
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleCursorPosition(out, a);
}
void CJZAPI SetPos(short x, short y) 
{ 
	COORD pos {x, y};
	SetPos0(pos);
}
VOID CJZAPI HideConsoleCursor() 
{
	CONSOLE_CURSOR_INFO cursor_info = {1, 0};
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}
VOID CJZAPI ShowConsoleCursor() 
{
	CONSOLE_CURSOR_INFO cursor_info = {1, 1};
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}
inline void CJZAPI cls()
{
	system("cls");
}
string ConvertErrorCodeToString(DWORD ErrorCode)  
{  
	HLOCAL LocalAddress {nullptr};  
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,  
		NULL, ErrorCode, 0, (PTSTR)&LocalAddress, 0, NULL);  
	return strxtail(LPSTR(LocalAddress), 2);  	//去除换行符
}  
inline int GetScreenHeight(void) //获取屏幕高度
{
	return GetSystemMetrics(SM_CYSCREEN);
}
inline int GetScreenWidth(void) //获取屏幕宽度
{
	return GetSystemMetrics(SM_CXSCREEN);
}
RECT CJZAPI GetSystemWorkAreaRect(void) //获取工作区矩形 
{
	RECT rt;
	SystemParametersInfo(SPI_GETWORKAREA,0,&rt,0);    // 获得工作区大小
	return rt;
}
LONG CJZAPI GetTaskbarHeight(void) 		//获取任务栏高度 
{	
	INT cyScreen = GetScreenHeight();
	RECT rt = GetSystemWorkAreaRect();
	return (cyScreen - (rt.bottom - rt.top));
}
inline HWND GetTaskbarWindow(void)
{
	return WindowFromPoint(POINT{ GetScreenWidth() / 2, GetScreenHeight() - 2 });
}
void ErrMessage(const string& s)
{
	SetColor(4, 0);
	cout << "错误：";
	SetColor(12, 0);
	cout << s;
	SetColor(15, 0);
	cout << "(";
	SetColor(14, 0);
	cout << ConvertErrorCodeToString(GetLastError());
	SetColor(15, 0);
	cout << ")\n";
}
bool CJZAPI ExistProcess(LPCSTR lpName)	//判断是否存在指定进程
{	//******警告！区分大小写！！！！******// 
	//*****警告！必须加扩展名！！！！*****// 
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);	
	if (INVALID_HANDLE_VALUE == hSnapshot) 	
	{		
		return false;	
	}	
	PROCESSENTRY32 pe = { sizeof(pe) };	
	BOOL fOk;	
	for (fOk = Process32First(hSnapshot, &pe); fOk; fOk = Process32Next(hSnapshot, &pe)) 	
	{		
		if (!stricmp(pe.szExeFile, lpName)) 		
		{			
			CloseHandle(hSnapshot);			
			return true;		
		}	
	}	
	return false;
}
BOOL CJZAPI IsRunAsAdmin(HANDLE hProcess=GetCurrentProcess()) 
{	//是否有管理员权限 
	BOOL bElevated = FALSE;  	
	HANDLE hToken = NULL;   	// Get current process token	
	if ( !OpenProcessToken(hProcess, TOKEN_QUERY, &hToken ) )		
		return FALSE; 	
	TOKEN_ELEVATION tokenEle;	
	DWORD dwRetLen = 0;   	// Retrieve token elevation information	
	if ( GetTokenInformation( hToken, TokenElevation, &tokenEle, sizeof(tokenEle), &dwRetLen ) ) 	
	{  		
		if ( dwRetLen == sizeof(tokenEle) ) 		
		{			
			bElevated = tokenEle.TokenIsElevated;  		
		}	
	}   	
	CloseHandle( hToken );  	
	return bElevated;  
} 
VOID CJZAPI AdminRun(LPCSTR csExe, LPCSTR csParam=NULL, DWORD nShow=SW_SHOW)  
{ 	//以管理员身份运行
	SHELLEXECUTEINFO ShExecInfo; 
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);  
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS ;  
	ShExecInfo.hwnd = NULL;  
	ShExecInfo.lpVerb = "runas";  
	ShExecInfo.lpFile = csExe/*"cmd"*/; 
	ShExecInfo.lpParameters = csParam;   
	ShExecInfo.lpDirectory = NULL;  
	ShExecInfo.nShow = nShow;  
	ShExecInfo.hInstApp = NULL;   
	BOOL ret = ShellExecuteEx(&ShExecInfo);  
	/*WaitForSingleObject(ShExecInfo.hProcess, INFINITE);  
	  GetExitCodeProcess(ShExecInfo.hProcess, &dwCode);  */
	CloseHandle(ShExecInfo.hProcess);
	return;
} //from https://blog.csdn.net/mpp_king/article/details/80194563

enum EventMajor {
	MousePosition,
	MouseOperation,
	KeyboardOperation,
	Check,
//	Assertion,
};
map<EventMajor, string> eventMajorStrings
{
	make_pair(MousePosition, "MP"s),
	make_pair(MouseOperation, "MO"s),
	make_pair(KeyboardOperation, "KB"s),
	make_pair(Check, "CHK"s),
};

enum MouseOperationType {
	LeftDown = 1,
	LeftUp,
	RightDown,
	RightUp,
	MiddleDown,
	MiddleUp,
	WheelDown,
	WheelUp,
};

enum CheckType {
	CheckWindowForeground,
	CheckWindowExist,
	CheckProcessExist,
};

void OnMouseWheel(int delta);

LRESULT CALLBACK RawInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_INPUT) {
		RAWINPUT raw;
		UINT size = sizeof(raw);
		GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
		if (raw.header.dwType == RIM_TYPEMOUSE) {
			if (raw.data.mouse.usButtonFlags == RI_MOUSE_WHEEL) {
				short wheelDelta = static_cast<short>(raw.data.mouse.usButtonData);
				if (wheelDelta > 0) {
					OnMouseWheel(-1);
				}
				else {
					OnMouseWheel(1);
				}
			}
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool g_wheel_detect{false};
DWORD MouseWheelDetectionProc(LPVOID none)
{
	WNDCLASSW wc = {};
	wc.lpfnWndProc = RawInputProc;
	wc.lpszClassName = L"RawInputDetectorClass";
	RegisterClassW(&wc);
	
	HWND hwnd = CreateWindowW(wc.lpszClassName, L"RawInputDetector", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
	
	RAWINPUTDEVICE Rid[1];
	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = RIDEV_INPUTSINK;
	Rid[0].hwndTarget = hwnd;
	RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));
	
	MSG msg;
	while (g_wheel_detect && GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	SendMessage(hwnd, WM_DESTROY, 0, 0);
	return 0;
}

struct KEY_CODE {
	BYTE code;
	vector<string> names;
};
vector<KEY_CODE> kcodes {};

string CJZAPI GetKeyName(BYTE bVk)
{
	for(auto& k : kcodes)
		if (k.code == bVk)
			if(k.names.size() > 0)
				return k.names.at(0);
			else
			{
//				SetColor(12, 0);
//				cout << "\n错误：没有名称的键 " << int(k.code) << endl;
//				return "未知";
				//return str(k.code);
				return str(int(bVk))+" ";
			}
	return "未知";
}
BYTE CJZAPI GetKeyByteByExpr(const string& k)
{
	if(kcodes.size() <= 0)
	{
		ErrMessage("按键字节码文件还未读取。");
		return 0;
	}
	for (int i = 0; i<kcodes.size(); i++)
	{
		if (kcodes.at(i).names.size() < 1)
			continue;
		for (int j = 0; j < kcodes.at(i).names.size(); j++)
		{
			if (sequ(k, kcodes.at(i).names.at(j).c_str()))
			{
				return kcodes.at(i).code;
			}
		}
	}
	return 0;
}

void ReadKeyCodes(void)
{	//不能有一个多余空格，否则当场卡死
	const string path = "KEY_CODE.txt";
	if (!ExistFile(path)) {
		SetColor(12, 0);
		cout << "错误：未找到按键字节码文件!\n";
		Sleep(1000);
		exit(0);
	}
	vector<string> lines = ReadFileLines(path);
	UINT i=0;
	while (i < lines.size()) 
	{
		string line = lines.at(i);
		if(line.empty())
			break; 
		vector<string> a = CutLine(line,' ');
	
		KEY_CODE kc;
		kc.code = ToInt(a.at(0));
//		cout << int(kc.code) << " ";
		if(kc.code < 0)
			break;
//		a.erase(a.begin());
		for(int j = 1; j < a.size(); ++j)
			kc.names.push_back(a.at(j));
//		cout << a.size() << "  " << kc.names.size() << "\t|\t";
		kcodes.push_back(kc);
		++i;
	}
	
//	SetColor(8, 0);
//	cout << "kcodes.size=" << kcodes.size() << endl;
}

#define STOP_DONE 0
#define STOP_KEYBD_INTERRUPT 1
#define STOP_CHECK_FAILURE 2

UINT g_stop_reason {0};
string g_stop_description{""};
bool g_stop_trigger {false};
bool g_paused{false};

struct MouseStatus
{
	bool left_down{false};
	bool right_down{false};
	bool middle_down{false};
	
	void Update()
	{
		left_down = KEY_DOWN(VK_LBUTTON);
		right_down = KEY_DOWN(VK_RBUTTON);
		middle_down = KEY_DOWN(VK_MBUTTON);
	}
};

struct KeyboardStatus
{
	array<bool, 255> keyboard_status;	//actually 7-254
					//0-UP 1-DOWN
	
	void Update()
	{
		for(BYTE bVk = 7; bVk <= 254; ++bVk)
			keyboard_status[bVk] = KEY_DOWN(bVk);
	}
};

#define CHECK_FAILURE_TEMPLATE(description) \
			if (operation_when_check_failure == CFOPER_NONE);	\
			else if(operation_when_check_failure == CFOPER_STOP)		\
			{		\
			g_stop_reason = STOP_CHECK_FAILURE;	\
			g_stop_description = description;	\
			g_stop_trigger = true;   	\
			}	\
			else if(operation_when_check_failure == CFOPER_WAIT)

class Event
{
	
public:
	EventMajor major {Check};
	clock_t timestamp {0L};	//相对值
	
	union EventUnion{
		
		POINT mp;
		
		MouseOperationType mo;
		
		struct {
			BYTE vk;
			bool pressed;
		}kb;
		
		struct {
			CheckType type;
			char s1[100];
			char s2[100];
		}chk;
	}u;
	
	static map<MouseOperationType, string> mo_string_map;

	bool Execute()
	{	//返回true时pop
		static map<MouseOperationType, BYTE> mo_map
		{
			pair{LeftDown, MOUSEEVENTF_LEFTDOWN},
			{LeftUp, MOUSEEVENTF_LEFTUP},
			{RightDown, MOUSEEVENTF_RIGHTDOWN},
			{RightUp, MOUSEEVENTF_RIGHTUP},
			{MiddleDown, MOUSEEVENTF_MIDDLEDOWN},
			{MiddleUp, MOUSEEVENTF_MIDDLEUP},
		};
		
		if (major == MousePosition)
		{
			SetCursorPos(u.mp.x, u.mp.y);
		}else if(major == MouseOperation)
		{
			if(u.mo != WheelDown && u.mo != WheelUp)
				mouse_event(mo_map[u.mo], 0, 0, 0, 0);
			else{
				INPUT input = {0};
				input.type = INPUT_MOUSE;
				input.mi.dwFlags = MOUSEEVENTF_WHEEL;
				input.mi.mouseData = (u.mo == WheelDown ? -1 : 1) * 120; // 120 表示向上滚动一次，-120 表示向下滚动一次
				SendInput(1, &input, sizeof(INPUT));
			}
		}else if(major == KeyboardOperation)
		{
			keybd_event(u.kb.vk, 0, u.kb.pressed?0:2, 0);
		}else if(major == Check)
		{
			switch(u.chk.type)
			{
			case CheckWindowForeground:{
				HWND hwnd_fore = GetForegroundWindow();
				bool failed { !hwnd_fore };
				if (!failed)
				{
					char caption[256]{0};
					GetWindowTextA(hwnd_fore, caption, 256);
					if (string{caption} != u.chk.s2)
					{
						failed = true;
					}else{
						char classname[256] {0};
						RealGetWindowClassA(hwnd_fore, classname, 256);
						failed = string{classname} != string{u.chk.s1};
					}
				}
				
				if (failed)
				{
					CHECK_FAILURE_TEMPLATE(("前端窗口不符合条件：[标题："s + u.chk.s2 + " 类名："s + u.chk.s1));
				}
				break;
			}
			case CheckProcessExist:{
				if (!ExistProcess(u.chk.s1))
				{
					CHECK_FAILURE_TEMPLATE(("进程 "s + u.chk.s1 + " 不存在"s));
				}
				break;
			}
			case CheckWindowExist:{
				if (nullptr == FindWindowA(u.chk.s1, u.chk.s2))
				{
					CHECK_FAILURE_TEMPLATE(("窗口（标题："s + u.chk.s2 + "， 类名："s + u.chk.s1 + "）不存在"));
				}
				break;
			}
			}
		}
		return true;
	}
	
	string ToString() const
	{
		stringstream ss;
		ss << eventMajorStrings[major] ;
		if (major == MousePosition)
			ss << "," << u.mp.x << "," << u.mp.y;
		else if (major == MouseOperation)
			ss << "," << u.mo;
		else if (major == KeyboardOperation)
			ss << "," << int(u.kb.vk) << "," << u.kb.pressed;
		else if (major == Check)
		{
			switch(u.chk.type)
			{
			case CheckWindowForeground:
			case CheckWindowExist:
				ss << "," << u.chk.s1 << "," << u.chk.s2;
				break;
			case CheckProcessExist:
				ss << "," << u.chk.s1;
				break;
			default:
				throw runtime_error{"无效的检查类型：" + str(u.chk.type)};
			}
		}
		
		ss << "@" << timestamp;
		
		return ss.str();
	}
	string ToDisplayString() const
	{
		if (major == MousePosition)
			return sprintf2("(%d,%d)", u.mp.x, u.mp.y);
		if (major == MouseOperation)
			return sprintf2("%s", mo_string_map[u.mo].c_str());
		if (major == KeyboardOperation)
			return sprintf2("%s%s", GetKeyName(u.kb.vk).c_str(), u.kb.pressed?"↓":"↑");
		return "?";
	}
	void OutputToConsole() const
	{
		if (major == MousePosition)
		{
			SetColor(7, 0);
			putchar(' ');
		}
		else if (major == MouseOperation)
		{
			switch(u.mo)
			{
			case LeftDown:
				SetColor(0, 8);
				cout << "LMS";
				SetColor(8, 0), cout << ' ';
				break;
			case LeftUp:
				SetColor(8, 0);
				cout << "LMS ";
				break;
			case RightDown:
				SetColor(0, 3);
				cout << "RMS";
				SetColor(3, 0), cout << ' ';
				break;
			case RightUp:
				SetColor(3, 0);
				cout << "RMS ";
				break;
			case MiddleDown:
				SetColor(0, 5);
				cout << "MMS";
				SetColor(5, 0), cout << ' ';
				break;
			case MiddleUp:
				SetColor(5, 0);
				cout << "MMS ";
				break;
			case WheelDown:
				SetColor(4, 0);
				cout << "W↓ ";
				break;
			case WheelUp:
				SetColor(2, 0);
				cout << "W↑ ";
				break;
			}
		}else if(major == KeyboardOperation)
		{
			if(u.kb.pressed)
			{
				SetColor(0, 8);
				cout << GetKeyName(u.kb.vk);
				SetColor(8, 0);
				cout << " ";
			}else{
				SetColor(8, 0);
				cout << GetKeyName(u.kb.vk) << " ";
			}
		}
	}
};

map<MouseOperationType, string> Event::mo_string_map =
{
	pair{LeftDown, "LMS↓"},
	{LeftUp, "LMS↑"},
	{RightDown, "RMS↓"},
	{RightUp, "RMS↑"},
	{MiddleDown, "MMS↓"},
	{MiddleUp, "MMS↑"},
	{WheelDown, "WHEEL↓"},
	{WheelUp, "WHEEL↑"},
};

#define RECENT_EVENTS_MAX_CNT 5
#define RECENT_EVENTS_SINGLE_SHOWN_TIME 2000.0

class EventManager {
	
public:
	vector<Event> events;			//录制时存储合并  运行时存储非位置
	vector<Event> _mouse_coords;	//录制时存储位置  运行时存储位置
	
	vector<Event> recent_events;	//近期事件 不存储鼠标位置
	
	MouseStatus mouse;
	KeyboardStatus keyboard;
	
	LONG prev_mx {0L}, prev_my {0L};
	LONG plan_mx, plan_my;
	clock_t prev_m_timestamp;
	
	clock_t start {0L};
	clock_t pause_total_duration {0L};
	
	void Reset(bool load = false)
	{
		if (!load)
		{
			_mouse_coords.clear();
			events.clear();
		}
		recent_events.clear();
		POINT pt{0, 0};
		GetCursorPos(&pt);
		prev_mx = pt.x , prev_my = pt.y;
		plan_mx = prev_mx , plan_my = prev_my;
		prev_m_timestamp = clock();
		UpdateMouseAndKeyboard();
		start = clock();
		pause_total_duration = 0L;
	}
	void AddRecentEvent(Event event)
	{
		recent_events.emplace_back(event);
		UpdateRecentEvents();
		event.OutputToConsole();
	}
	void UpdateRecentEvents()
	{
		while (recent_events.size() > RECENT_EVENTS_MAX_CNT
			|| !recent_events.empty() && clock() - start - recent_events.front().timestamp > RECENT_EVENTS_SINGLE_SHOWN_TIME)
			recent_events.erase(recent_events.begin());
	}
	void AddMousePosition(LONG mx, LONG my)
	{
		Event event;
		event.major = MousePosition;
		event.u.mp.x = mx;
		event.u.mp.y = my;
		event.timestamp = clock() - start - pause_total_duration;
		events.emplace_back(event);
		_mouse_coords.emplace_back(event);
		cout << " ";
	}
	void AddMousePosition(void)
	{
		POINT pt;
		GetCursorPos(&pt);
		AddMousePosition(pt.x, pt.y);
	}
	void AddMouseOperation(MouseOperationType oid, bool fake = false)
	{
		if (!fake)
			AddMousePosition();
		
		Event event;
		event.major = MouseOperation;
		event.u.mo = oid;
		event.timestamp = clock() - start - pause_total_duration;
		if(!fake)
			events.emplace_back(event);
		AddRecentEvent(event);
	}
	void AddKeyboardOperation(BYTE bVk, bool down, bool fake = false)
	{
		Event event;
		event.major = KeyboardOperation;
		event.u.kb.vk = bVk;
		event.u.kb.pressed = down;
		event.timestamp = clock() - start - pause_total_duration;
		if (!fake)
			events.emplace_back(event);
		AddRecentEvent(event);
	}
	void KeepRunning()
	{
		if (g_ui_stage != UIExecuting)
			return;
		if (events.empty() && _mouse_coords.empty())
			return;
		
		if (events.front().timestamp <= clock() - start)
		{
			AddRecentEvent(events.front());
			if (events.front().Execute())
				events.erase(events.begin());
			//否则一直执行
		}
		
		if(_mouse_coords.empty())
			return;
		
		if (_mouse_coords.front().timestamp <= clock() - start)
		{
			//_mouse_coords.front().Execute();
			prev_mx = _mouse_coords.front().u.mp.x;
			prev_my = _mouse_coords.front().u.mp.y;
//			prev_mx = plan_mx;
//			prev_my = plan_my;
			prev_m_timestamp = _mouse_coords.front().timestamp;
			_mouse_coords.erase(_mouse_coords.begin());
		}
		
		double _rat = Clamp((clock() - start - prev_m_timestamp) / double(_mouse_coords.front().timestamp - prev_m_timestamp), 0.0, 1.0);
//		double _rat = (clock() - prev_m_timestamp) / double(_mouse_coords.front().timestamp - prev_m_timestamp);
		plan_mx = Lerp<int>(prev_mx, _mouse_coords.front().u.mp.x, EaseInOutSine(_rat));
		plan_my = Lerp<int>(prev_my, _mouse_coords.front().u.mp.y, EaseInOutSine(_rat));
//		plan_mx = prev_mx;
//		plan_my = prev_my;
		
//		cout << "prev_mx=" << prev_mx << " target_mx=" <<_mouse_coords.front().u.mp.x << "  _rat=" << _rat << endl;
		SetCursorPos(plan_mx, plan_my);
	}
	void UpdateMouseAndKeyboard()
	{
		mouse.Update();
		keyboard.Update();
	}
	void MonitorCheckEvents(HWND& hwnd_fore)
	{
		HWND hwnd_now_fore = GetForegroundWindow();
		if (hwnd_fore != hwnd_now_fore)
		{
			char buffer[256]{0};
			
			Event event;
			event.timestamp = clock();
			event.major = Check;
			event.u.chk.type = CheckWindowForeground;
			GetWindowTextA(hwnd_now_fore, buffer, 256);
//			event.u.chk.s1 = string(buffer);
			strncpy(event.u.chk.s1, buffer, 100);
			RealGetWindowClassA(hwnd_now_fore, buffer, 256);
//			event.u.chk.s2 = string(buffer);
			strncpy(event.u.chk.s2, buffer, 100);
			
			events.emplace_back(event);
			hwnd_fore = hwnd_now_fore;
		}
	}
	void MonitorMouseButtons(bool fake = false)
	{
		if(KEY_DOWN(VK_LBUTTON) && !mouse.left_down)
		{
			mouse.left_down = true;
			AddMouseOperation(LeftDown, fake);
		}else if(!KEY_DOWN(VK_LBUTTON) && mouse.left_down)
		{
			mouse.left_down = false;
			AddMouseOperation(LeftUp, fake);
		}
		
		if(KEY_DOWN(VK_RBUTTON) && !mouse.right_down)
		{
			mouse.right_down = true;
			AddMouseOperation(RightDown, fake);
		}else if(!KEY_DOWN(VK_RBUTTON) && mouse.right_down)
		{
			mouse.right_down = false;
			AddMouseOperation(RightUp, fake);
		}
		
		if(KEY_DOWN(VK_MBUTTON) && !mouse.middle_down)
		{
			mouse.middle_down = true;
			AddMouseOperation(MiddleDown, fake);
		}else if(!KEY_DOWN(VK_MBUTTON) && mouse.middle_down)
		{
			mouse.middle_down = false;
			AddMouseOperation(MiddleUp, fake);
		}
		
	}
	void MonitorKeyboard(bool fake = false)
	{
		for(BYTE vk = 7; vk <= 254; ++vk)
		{
			if (KEY_DOWN(vk) && !keyboard.keyboard_status[vk])
			{
				keyboard.keyboard_status[vk] = true;
				AddKeyboardOperation(vk, true, fake);
			}else if (!KEY_DOWN(vk) && keyboard.keyboard_status[vk])
			{
				keyboard.keyboard_status[vk] = false;
				AddKeyboardOperation(vk, false, fake);
			}
		}
	}
	void BalanceEvents()
	{
		for(BYTE bVk = 7; bVk <= 254; ++bVk)
			if(keyboard.keyboard_status[bVk])
			{
				AddKeyboardOperation(bVk, false);
			}
		if(mouse.left_down)
			AddMouseOperation(LeftUp);
		if(mouse.right_down)
			AddMouseOperation(RightUp);
		if(mouse.middle_down)
			AddMouseOperation(MiddleUp);
	}
	bool SaveToFile(const string& filename)
	{
		if (!ExistFile("Record"))
			CreateDirectoryA("Record", nullptr);
		
		fstream fout(filename,
			ios::out);
		if (!fout)
		{
			ErrMessage("无法打开文件");
			return false;
		}
		
		try
		{
			for(auto const& e : events)
			{
				fout << e.ToString() << endl;
			}
			
		}catch(exception& e)
		{
			SetColor(12, 0);
			cout << "错误：" << e.what() << '\n';
			fout.close();
			return false;
		}
		fout.close();
		return true;
	}
	bool LoadFromFile(const string& path)
	{
		if (!ExistFile("Record\\"+path))
		{
			SetPos(0, 0);
			SetColor(12, 0);
			cout << "   错误：路径无效    \n";
			Sleep(1000);
			return false;
		}
		SetColor(7, 0);
		cout << "\n读取脚本文件 ";
		SetColor(5, 0);
		cout << "Record\\" + path + "\n";
		vector<string> lines = ReadFileLines("Record\\"+path);
		int i{};
		
		try{
			while (i < lines.size())
			{
				string line = lines.at(i);
				vector<string> cut0 = CutLine(line, '@');
				if(cut0.size() < 2)
				{
					++i;
					continue;
				}
				
				Event event;
				event.timestamp = ToInt(cut0[1]);
				vector<string> cut1 = CutLine(cut0[0], ',');
				
				bool found {false};
				for(const auto& pr : eventMajorStrings)
				{
					if(sequ(pr.second, cut1[0]))
					{
						found = true;
						event.major = pr.first;
						break;
					}
				}
				if (!found)
				{
					throw runtime_error("无效的事件名称：" + cut1[0]);
					continue;
				}
				if (event.major == MousePosition)
				{
					event.u.mp.x = ToInt(cut1[1]);
					event.u.mp.y = ToInt(cut1[2]);
				}else if(event.major == MouseOperation)
				{
					event.u.mo = MouseOperationType(ToInt(cut1[1]));
				}else if(event.major == KeyboardOperation)
				{
					event.u.kb.vk = ToInt(cut1[1]);
					event.u.kb.pressed = ToInt(cut1[2]);
				}else if(event.major == Check)
				{
					event.u.chk.type = (CheckType)ToInt(cut1[1]);
					switch(event.u.chk.type)
					{
					case CheckWindowForeground:{
						strncpy(event.u.chk.s1, cut1[2].c_str(), 100);
						strncpy(event.u.chk.s2, cut1[3].c_str(), 100);
						break;
					}
					case CheckProcessExist:{
						strncpy(event.u.chk.s1, cut1[2].c_str(), 100);
						break;
					}
					case CheckWindowExist:{
						strncpy(event.u.chk.s1, cut1[2].c_str(), 100);
						strncpy(event.u.chk.s2, cut1[3].c_str(), 100);
						break;
					}
					default:{
						throw runtime_error{"无效的检查类型：" + str(event.u.chk.type)};
						break;
					}
					}
				}
				
				if(event.major == MousePosition)
					_mouse_coords.emplace_back(event);
				else
					events.emplace_back(event);
				++i;
			}
		}catch(exception& e)
		{
			SetColor(12, 0);
			cout << "错误：" << e.what() << endl;
			cout << "读取脚本文件失败\n";
			return false;
		}
		return true;
	}
}eventMgr;

void OnMouseWheel(int delta)
{
	if (g_paused)
		return;
	eventMgr.AddMouseOperation(delta>0?WheelDown:WheelUp);
}

//AA,...@timestamp
/*
  AA:   MP-鼠标位置   
            MP,X,Y  
        MO-鼠标操作
            MO,B
  
		    B: 1-LEFTDOWN  2-LEFTUP  3-RIGHTDOWN  4-RIGHTUP
                5-MIDDLEDOWN 6-MIDDLEUP 7-WHEELDOWN 8-WHEELUP
        KB-键盘操作
	        KB,CCC,D
            
		    CCC: 字节码 (VK_)
            D: 0-UP 1-DOWN
 */

bool EnterPause()
{
	g_ui_stage = UIRecordPause;
	SetConsoleTitleA("已暂停录制 - UserStimulator   @Wormwaker");
	SetColor(2, 0);
	cout << "\n---------------------------------\n";
	SetColor(5, 0);
	cout << "              录制已暂停           \n\n";
	SetColor(15, 0);
	cout << "[Alt+1] 继续录制\n\n";
	cout << "[Alt+2] 结束录制\n\n";
	
	while(1)
	{
		if(KEY_DOWN(VK_LMENU))
		{
			if(KEY_DOWN('1'))
			{
				while(KEY_DOWN(VK_LMENU) && KEY_DOWN('1'));
				MessageBeep(MB_ICONEXCLAMATION);
				return true;
			}else if(KEY_DOWN('2'))
			{
				while(KEY_DOWN(VK_LMENU) && KEY_DOWN('2'));
				MessageBeep(MB_ICONEXCLAMATION);
				return false;
			}
		}
		
		MSG msg;
		if (GetMessage(&msg, NULL, 0, 0) > 0) { /* If no error is received... */
			TranslateMessage(&msg); 			/* Translate key codes to chars if present */
			DispatchMessage(&msg); 				/* Send it to WndProc */
		}
	}
	return true;
}
#define RECENT_EVENTS_UPDATE_CD 200.0
void RecordOperations()
{
	g_ui_stage = UIRecordPre;
	SetConsoleTitleA("录制脚本 - UserStimulator   @Wormwaker");
	cls();
	
	TipWindowSwitch(true);
	SetColor(14, 0);
	cout << "-------------------------------\n";
	SetColor(10, 0);
	cout << "           录制新操作          \n\n";
	SetColor(15, 0);
	cout << "[Alt+1] 开始/暂停录制\n\n";
	cout << "[Alt+2] 结束录制\n\n";
	SetColor(8, 0);
	cout << "Waiting for [Alt+1] ... or press [Esc] to cancel";
	while(1)
	{
		if(K(VK_ESCAPE))
		{
			while(K(VK_ESCAPE));
			FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
			SetColor(12, 0);
			cout << "\n\n已取消！";
			TipWindowSwitch(false);
			Sleep(500);
			return;
		}
		else if(K(VK_MENU))
		{
			if(K('1'))
			{
				while(K(VK_MENU) && K('1'));
				break;
			}
		}
	}
	
	g_ui_stage = UIRecording;
	SetConsoleTitleA("●正在录制 - UserStimulator   @Wormwaker");
	MessageBeep(MB_ICONEXCLAMATION);
	
	cls();
	SetColor(4, 0),	cout << "-------------------------------\n";
	SetColor(12, 0),cout << "  ●";
	SetColor(7, 0),	cout <<      "REC";
	SetColor(15, 0),cout <<          "  正在录制          \n\n";
	SetColor(12, 0);
	cout << "[Alt+1] 暂停录制\n\n";
	cout << "[Alt+2] 结束录制\n\n";
	
	HWND hwnd_fore{nullptr};
	LONG prev_mx{0L}, prev_my{0L};
	LONG mx{0L}, my{0L};
	POINT pt {0, 0};
	
	GetCursorPos(&pt);
	prev_mx = pt.x, prev_my = pt.y;
	eventMgr.Reset();	//开始计时
	
	SendMessage(hwnd_console, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	g_wheel_detect = true;
	g_stop_trigger = false;
	g_stop_description = "";
	g_stop_reason = STOP_DONE;
	CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MouseWheelDetectionProc, nullptr, 0, nullptr);
	
	clock_t lastSample { clock() };
	clock_t lastSmallSample { clock() };
	clock_t lastREventUpdate {clock()};
	
	while(1)
	{
		if(clock() - lastSample > SAMPLE_RECORD_CD)
		{
			GetCursorPos(&pt);
			if (pt.x != prev_mx || pt.y != prev_my)
			{
				prev_mx = mx, prev_my = my;
				mx = pt.x, my = pt.y;
				eventMgr.AddMousePosition(mx, my);
			}
			
			lastSample = clock();
		}else if(clock() - lastSmallSample > SMALL_SAMPLE_RECORD_CD)
		{
			GetCursorPos(&pt);
			if (pt.x != prev_mx || pt.y != prev_my)
			{
				prev_mx = mx, prev_my = my;
				mx = pt.x, my = pt.y;
			}
			if(Distance(mx, my, prev_mx, prev_my) > SMALL_SAMPLE_MIN_DISTANCE)
			{
				eventMgr.AddMousePosition(mx, my);
				lastSample = clock();
			}
		}
		
		if(clock() - lastREventUpdate >= RECENT_EVENTS_UPDATE_CD)
		{
			eventMgr.UpdateRecentEvents();
			lastREventUpdate = clock();
		}
		
		MSG msg;
		if (GetMessage(&msg, NULL, 0, 0) > 0) { /* If no error is received... */
			TranslateMessage(&msg); 			/* Translate key codes to chars if present */
			DispatchMessage(&msg); 				/* Send it to WndProc */
		}
		
		eventMgr.MonitorCheckEvents(hwnd_fore);
		eventMgr.MonitorMouseButtons();
		eventMgr.MonitorKeyboard();
		
		if(KEY_DOWN(VK_MENU))
		{
			if(KEY_DOWN('1'))
			{
				while(KEY_DOWN(VK_LMENU) && KEY_DOWN('1'));
			    MessageBeep(MB_ICONEXCLAMATION);
				
				g_paused = true;
				SendMessage(hwnd_console, WM_SYSCOMMAND, SC_RESTORE, 0);
				bool doContinue = EnterPause();
				g_paused = false;
				
				if(!doContinue)
				{
					g_wheel_detect = false;
					break;
				}
				g_ui_stage = UIRecording;
				SendMessage(hwnd_console, WM_SYSCOMMAND, SC_MINIMIZE, 0);
				SetColor(10, 0);
				cout << "\n继续录制！";
				SetConsoleTitleA("●正在录制 - UserStimulator   @Wormwaker");
				SetColor(12, 0);
				cout << "\n----------------------------------------\n";
				MessageBeep(MB_ICONEXCLAMATION);
			}else if(KEY_DOWN('2'))
			{
				while(KEY_DOWN(VK_LMENU) && KEY_DOWN('2'));
				g_stop_description = "快捷键停止";
				g_stop_reason = STOP_KEYBD_INTERRUPT;
				MessageBeep(MB_ICONEXCLAMATION);
				g_wheel_detect = false;
				break;
			}
		}
		
		if (g_stop_trigger)
		{
			g_stop_trigger = false;
			break;
		}
	}
	g_ui_stage = UIRecordEnd;
	TipWindowSwitch(false);
	SetConsoleTitleA("保存录制脚本 - UserStimulator   @Wormwaker");
	SetColor(10, 0);
	cout << "\n-----------------------------------------\n";
	SetColor(14, 0);
	cout << "录制完毕！关键帧数量：";
	SetColor(11, 0);
	cout << eventMgr.events.size() << endl;
	
	eventMgr.BalanceEvents();	//平衡
	
//	SetColor(12, 0);
//	cout << "\n[!] 请按下 ";
//	SetColor(11, 0);
//	cout << "回车键 ";
//	SetColor(12, 0);
//	cout << "以继续 ";
//	SetColor(1, 0);
//	HANDLE hPrevOutput{nullptr};
//	SetStdHandleEx(STD_OUTPUT_HANDLE, nullptr, &hPrevOutput);
//	cin.clear();
//	cin.ignore(numeric_limits<streamsize>::max(), '\n');
//	SetStdHandle(STD_OUTPUT_HANDLE, hPrevOutput);
	
	//唯一真神
	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
	
	SetColor(15, 0);
	cout << "\n输入短文件名";
	SetColor(7, 0);
	cout << "（包括后缀名，推荐.steps）：";
	SetColor(13, 0);
	char line[MAX_PATH]{0};
	
//	char nope[1024];
//	cin.get(nope, 1024);
//	cin.putback('\n');
	cin.getline(line, MAX_PATH);
	SetColor(14, 0);
	cout << "\n保存操作至：Record\\";
	SetColor(12, 0);
	cout << line << endl;
	bool bRet = eventMgr.SaveToFile("Record\\"s + line);
	if (bRet)
	{
		SetColor(10, 0);
		cout << "保存完毕\n\n";
	}
	SetColor(8, 0);
	cout << " [请按任意键继续] ";
	getch();
	eventMgr.events.clear();
	eventMgr._mouse_coords.clear();
	eventMgr.recent_events.clear();
	cls();
}

void DisplayOperations()
{
	cls();
	SetColor(14, 0);
	cout << "-----------------------------------------------------\n";
	SetColor(10, 0);
	cout << "                      开始展示                       \n\n";
	SetColor(3, 0);
	cout << "已经开始了！\n\n";
	SetColor(12, 0);
	cout << "[Alt+1] 暂停\n\n";
	SetColor(12, 0);
	cout << "[Alt+2] 关闭\n\n";
	
	TipWindowSwitch(true);
	SetConsoleTitleA("●正在展示 - UserStimulator   @Wormwaker");
	MessageBeep(MB_ICONEXCLAMATION);
	g_ui_stage = UIDisplaying;
	
	POINT pt;
	int prev_mx, prev_my;
	int mx, my;
	clock_t lastSample{clock()};
	clock_t lastSmallSample{clock()};
	clock_t lastREventUpdate{clock()};
	
	while(1)
	{
		if(clock() - lastSample > SAMPLE_RECORD_CD)
		{
			GetCursorPos(&pt);
			if (pt.x != prev_mx || pt.y != prev_my)
			{
				prev_mx = mx, prev_my = my;
				mx = pt.x, my = pt.y;
				eventMgr.AddMousePosition(mx, my);
			}
			
			lastSample = clock();
		}else if(clock() - lastSmallSample > SMALL_SAMPLE_RECORD_CD)
		{
			GetCursorPos(&pt);
			if (pt.x != prev_mx || pt.y != prev_my)
			{
				prev_mx = mx, prev_my = my;
				mx = pt.x, my = pt.y;
			}
		}
		
		if(clock() - lastREventUpdate >= RECENT_EVENTS_UPDATE_CD)
		{
			eventMgr.UpdateRecentEvents();
			lastREventUpdate = clock();
		}
		
		MSG msg;
		if (GetMessage(&msg, NULL, 0, 0) > 0) { /* If no error is received... */
			TranslateMessage(&msg); 			/* Translate key codes to chars if present */
			DispatchMessage(&msg); 				/* Send it to WndProc */
		}
		
//		eventMgr.MonitorCheckEvents();
		eventMgr.MonitorMouseButtons(true);
		eventMgr.MonitorKeyboard(true);
		
		if(KEY_DOWN(VK_MENU))
		{
			if(KEY_DOWN('1'))
			{
				while(KEY_DOWN(VK_LMENU) && KEY_DOWN('1'));
				MessageBeep(MB_ICONEXCLAMATION);
				
				g_paused = true;
				SendMessage(hwnd_console, WM_SYSCOMMAND, SC_RESTORE, 0);
				bool doContinue = EnterPause();
				g_paused = false;
				
				if(!doContinue)
				{
					g_wheel_detect = false;
					break;
				}
				g_ui_stage = UIDisplaying;
				SendMessage(hwnd_console, WM_SYSCOMMAND, SC_MINIMIZE, 0);
				SetColor(10, 0);
				cout << "\n继续展示！";
				SetConsoleTitleA("●正在展示 - UserStimulator   @Wormwaker");
				SetColor(12, 0);
				cout << "\n----------------------------------------\n";
				MessageBeep(MB_ICONEXCLAMATION);
			}else if(KEY_DOWN('2'))
			{
				while(KEY_DOWN(VK_LMENU) && KEY_DOWN('2'));
				g_stop_description = "快捷键停止";
				g_stop_reason = STOP_KEYBD_INTERRUPT;
				MessageBeep(MB_ICONEXCLAMATION);
				g_wheel_detect = false;
				break;
			}
		}
		
		if (g_stop_trigger)
		{
			g_stop_trigger = false;
			break;
		}
	}
	
	eventMgr.events.clear();
	eventMgr._mouse_coords.clear();
	eventMgr.recent_events.clear();
	cls();
}

void EnterRunOperations(const string& path)
{
	cls();
	if (!eventMgr.LoadFromFile(path))
		return;
	g_ui_stage = UIExecutePre;
	SetConsoleTitleA("运行脚本 - UserStimulator   @Wormwaker");	
	cls();
	SetColor(10, 0);
	cout << "\n操作加载完毕！\n";
	SetColor(2, 0);
	cout << "共计 ";
	SetColor(14, 0);
	cout << eventMgr.events.size();
	SetColor(2, 0);
	cout << " 个操作\n";
	
	SetColor(12, 0);
	cout << "\n[Alt+1] 开始运行\n";
	cout << "[Alt+2] 终止运行\n\n";
	SetColor(8, 0);
	cout << "Waiting for [Alt+1], or press [Esc] to cancel...";
	
	while(1)
	{
		if(KEY_DOWN(VK_ESCAPE))
		{
			SetColor(12, 0);
			cout << "\r已取消运行！";
			Sleep(500);
			cls();
			return;
		}
		if(KEY_DOWN(VK_MENU))
		{
			if(KEY_DOWN('1'))
			{
				while(KEY_DOWN(VK_MENU) && KEY_DOWN('1'));
				MessageBeep(MB_ICONEXCLAMATION);
				SetColor(10, 0);
				cout << "\r-------------------------------------------\n";
				SetColor(14, 0);
				cout << "            操作脚本已经开始运行！              \n\n\n";
				break;
			}
		}
	}
	g_ui_stage = UIExecuting;
	g_stop_description = "";
	g_stop_reason = STOP_DONE;
	g_stop_trigger = false;
	SetConsoleTitleA("▲正在运行 - UserStimulator   @Wormwaker");
	SendMessage(hwnd_console, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	TipWindowSwitch(true);
	eventMgr.Reset(true);
	
	clock_t lastREventUpdate{clock()};
	while(1)
	{
		if(clock() - lastREventUpdate >= RECENT_EVENTS_UPDATE_CD)
		{
			eventMgr.UpdateRecentEvents();
			lastREventUpdate = clock();
		}
		eventMgr.KeepRunning();
		
		MSG msg;
		if (GetMessage(&msg, NULL, 0, 0) > 0) { /* If no error is received... */
			TranslateMessage(&msg); 			/* Translate key codes to chars if present */
			DispatchMessage(&msg); 				/* Send it to WndProc */
		}
		
		if(eventMgr.events.empty())
		{
			SetColor(10, 0);
			cout << "\n--------------------------------------------\n";
			SetColor(11, 0);
			cout << "  脚本执行完毕！耗时：";
			SetColor(12, 0);
			printf("%.3f", (clock() - eventMgr.start) / (double)CLOCKS_PER_SEC);
			SetColor(11, 0);
			cout << "s\n";
			break;
		}
		if(KEY_DOWN(VK_MENU) && KEY_DOWN('2'))
		{
			while(KEY_DOWN(VK_MENU) && KEY_DOWN('2'));
			g_stop_reason = STOP_KEYBD_INTERRUPT;
			g_stop_description = "快捷键终止";
			MessageBeep(MB_ICONEXCLAMATION);
			SetColor(12, 0);
			cout << "\n---------------------------------\n";
			SetColor(3, 0);
			cout << "终止运行！\n";
			Sleep(800);
			break;
		}
		
		if(g_stop_trigger)
		{
			g_stop_trigger = false;
			break;
		}
	}
	
	g_ui_stage = UIExecuteEnd;
	TipWindowSwitch(false);
	SendMessage(hwnd_console, WM_SYSCOMMAND, SC_RESTORE, 0);
	SetConsoleTitleA("已停止运行 - UserStimulator   @Wormwaker");
	if (g_stop_reason != STOP_DONE)
	{
		static string stop_reason_strings[]
		{
			"执行完毕",
			"快捷键中止",
			"断言失败",
		};
		SetColor(15, 0);
		cout << "运行中止。原因：";
		SetColor(13, 0);
		cout << stop_reason_strings[g_stop_reason];
		if (!g_stop_description.empty())
		{
			SetColor(8, 0), cout << " (";
			SetColor(2, 0), cout << g_stop_description;
			SetColor(8, 0), cout << ")";
		}
		putchar('\n');
	}
	SetColor(8, 0);
	cout << "\n [请按任意键继续]  ";
	FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
	getch();
	eventMgr.events.clear();
	eventMgr._mouse_coords.clear();
	eventMgr.recent_events.clear();
	cls();
}

struct FileAnalysis {
	
};

void DrawBrowseOperationFile(const FileAnalysis& fana)
{
	SetPos(0, 0);
	SetColor(7, 0);
}
void BrowseOperationFile()
{
	 
}

struct OperationFilesSelection
{
	int cur_page {0};
	int cur_i {0};
};

#define FILELIST_PAGE_ITEM_CNT 8
#define FILELIST_PAGE_TOTAL_CNT (ceil(files.size() / (double)FILELIST_PAGE_ITEM_CNT))

void DrawOperationFiles(const vector<string>& files, const OperationFilesSelection& uidata)
{
	if (files.empty())
	{
		SetPos(0, 0);
		SetColor(12, 0);
		cout << "\n还没有任何录制的操作文件（.steps）！";
		SetColor(8, 0);
		cout << "[Esc] 返回  ";
		return;
	}
	
	SetPos(0, 0);
	SetColor(15, 0);
	cout << "                                    录制的操作文件            \n";
	SetColor(2, 0);
	cout << "                                     P " << uidata.cur_page + 1 << " of " << FILELIST_PAGE_TOTAL_CNT << "      " << endl;
	SetColor(3, 0);
	cout << "    ";
	
	size_t namelen = 0U;
	for(int k{uidata.cur_page * FILELIST_PAGE_ITEM_CNT}; 
		    k < files.size() 
		    && k < (uidata.cur_page+1) * FILELIST_PAGE_ITEM_CNT; 
		++k)
		namelen = max(files.at(k).length(), namelen);
	namelen = max(namelen + 2, 10ZU);
	
	for(short _ = 0; _ < namelen + 60; ++_)
		putchar('-');
	cout << "\n\n";
	
	for(int i{uidata.cur_page * FILELIST_PAGE_ITEM_CNT}; 
		i < (uidata.cur_page+1) * FILELIST_PAGE_ITEM_CNT; 
		++i)
	{
		if (i >= files.size())
		{
			SetColor(7, 0);
			printf("    %*c\n\n", namelen + 38, ' ');
			continue;
		}
		const string& name = files.at(i);
		if(i == uidata.cur_i)
			SetColor(0, 7);
		else
			SetColor(7, 0);
		
		size_t fsize = 0U;
		string s_time = "";
		
		printf("    %*s", namelen, name.c_str());
		
		if(i == uidata.cur_i)
			SetColor(5, 7);
		else
			SetColor(7, 0);
		if (GetFileSize("Record\\"+name, fsize))
		{
			string s_fsize = FileSizeString(fsize);
			printf("  %11s", (s_fsize).c_str());
		}
		else
			printf("  %11s", "ERROR");
		
		if(i == uidata.cur_i)
			SetColor(4, 7);
		else
			SetColor(7, 0);
		
		if (GetFileModifyTimeString("Record\\"+name, s_time))
			printf("  %20s", s_time.c_str());
		else
			printf("  %20s", "UNKNOWN");
		
		cout << "\n\n";
	}
	SetPos(3, 5 + 2 * FILELIST_PAGE_ITEM_CNT);
	SetColor(2, 0);
	cout << "\n\n 共计 ";
	SetColor(14, 0);
	cout << files.size();
	SetColor(2, 0);
	cout << " 个文件";
	
	int rui_left = namelen + 50;
	SetPos(rui_left, 5);
	SetColor(12, 0);
	cout << "[Esc] 返回 ";
	SetPos(rui_left, 7);
	SetColor(3, 0);
	cout << "[Space] 浏览  ";
	SetPos(rui_left, 9);
	SetColor(2, 0);
	cout << "[Enter] 运行 ";
	SetPos(rui_left, 11);
	SetColor(8, 0);
	cout << "[Delete] 删除 ";
}
#define DRAW_CD 60
#define READ_FILES_CD 3000
void OpenOperations()
{
	g_ui_stage = UIFileManage;
	SetConsoleTitleA("管理脚本 - UserStimulator   @Wormwaker");
	cls();
	vector<string> files = GetDirFilesR("Record\\", "*.steps");
	
	clock_t lastDraw{clock()};
	clock_t lastRead{clock()};
	OperationFilesSelection uidata {.cur_page = 0, .cur_i = 0};
	HideConsoleCursor();
	while(1)
	{
		if(clock() - lastDraw >= DRAW_CD)
		{
			DrawOperationFiles(files, uidata);
			lastDraw = clock();
		}
		if(clock() - lastRead >= READ_FILES_CD)
		{
			files = GetDirFilesR("Record\\", "*.steps");
//			cls();
			lastRead = clock();
		}
		
		if(K(VK_DOWN) || K('S'))
		{
			while(K(VK_DOWN) || K('S'));
			if(uidata.cur_i < int(files.size()) - 1)
			{
				uidata.cur_i++;
				if (uidata.cur_i >= (uidata.cur_page+1) * FILELIST_PAGE_ITEM_CNT)
					uidata.cur_page++;
			}
		}else if(K(VK_UP) || K('W'))
		{
			while(K(VK_UP) || K('W'));
			if(uidata.cur_i > 0)
			{
				uidata.cur_i--;
				if (uidata.cur_i < (uidata.cur_page) * FILELIST_PAGE_ITEM_CNT)
					uidata.cur_page--;
			}
		}else if(K(VK_NEXT))
		{
			while(K(VK_NEXT));
			if (uidata.cur_page < FILELIST_PAGE_TOTAL_CNT - 1)
			{
				uidata.cur_page++;
				uidata.cur_i += FILELIST_PAGE_ITEM_CNT;
				ClampA<int>(uidata.cur_i, 0, files.size() - 1);
			}
		}else if(K(VK_PRIOR))
		{
			while(K(VK_PRIOR));
			if (uidata.cur_page > 0)
			{
				uidata.cur_page--;
				uidata.cur_i -= FILELIST_PAGE_ITEM_CNT;
				ClampA<int>(uidata.cur_i, 0, files.size() - 1);
			}
		}
		else if(K(VK_ESCAPE))
		{
			while(K(VK_ESCAPE));
			FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
			MessageBeep(MB_ICONEXCLAMATION);
			break;
		}else if(K(VK_DELETE))
		{
			while(K(VK_DELETE));
			if (!DeleteFileA(("Record\\"+files.at(uidata.cur_i)).c_str()))
				MessageBeep(MB_ICONERROR);
			else
			{
				files = GetDirFilesR("Record\\", "*.steps");
//				cls();
				ClampA<int>(uidata.cur_i, 0, files.size() - 1);
				lastRead = clock();
			}
		}else if(K(VK_SPACE))
		{
			while(K(VK_SPACE));
			BrowseOperationFile();
			MessageBeep(MB_ICONEXCLAMATION);
			g_ui_stage = UIFileManage;
		}
		else if(K(VK_RETURN))
		{
			while(K(VK_RETURN));
			EnterRunOperations(files.at(uidata.cur_i));
			MessageBeep(MB_ICONEXCLAMATION);
			g_ui_stage = UIFileManage;
		}
	}
	ShowConsoleCursor();
	cls();
}

void DrawMainUI()
{
	SetConsoleTitleA("UserStimulator   @Wormwaker");
	SetColor(14, 0);
	cout << "==========================\n";
	SetColor(10, 0);
	cout << "      UserStimulator      \n";
	SetColor(3, 0);
	cout << "         @Wormwaker       \n\n";
	SetColor(7, 0);
	cout << "按下：\n";
	SetColor(15, 0), cout << "[R] ";
	SetColor(2, 0), cout << "录制新操作 \n\n";
	SetColor(15, 0), cout << "[D] ";
	SetColor(2, 0), cout << "实时展示当前操作 \n\n";
	SetColor(15, 0), cout << "[O] ";
	SetColor(2, 0), cout << "查阅已录制的操作 \n\n";
	if (!g_admin)
	{
		SetColor(15, 0);
		cout << "[A]  ";
		SetColor(2, 0);
		cout << "以管理员权限重启 \n\n";
	}
	SetColor(15, 0), cout << "[Esc] ";
	SetColor(2, 0), cout << "退出程序 \n\n";
	SetColor(3, 0);
	cout << "> ";
}
void EnterMainUI()
{
	g_ui_stage = UIMain;
	DrawMainUI();
	
	int ch;
	do{
		ch = getch();
		
		switch(ch)
		{
		case 27:
			return;
		case 'R': case 'r':
			RecordOperations();
			break;
		case 'D': case 'd':
			DisplayOperations();
			break;
		case 'O': case 'o':
			OpenOperations();
			break;
		case 'A':case 'a':
			if (!g_admin)
			{
				AdminRun(_pgmptr, nullptr, SW_SHOWNORMAL);
				SetColor(12, 0);
				cout << "\n\n退出...";
				SendMessage(hwnd, WM_DESTROY, 0, 0);
				Sleep(500);
				exit(0);
				break;
			}
		default:
			continue;
		}
		g_ui_stage = UIMain;
		DrawMainUI();
	}while(1);
}

BOOL WINAPI CtrlHandler(DWORD event)
{
	switch(event)
	{
	case CTRL_C_EVENT:
		return TRUE;
	case CTRL_BREAK_EVENT:
		return TRUE;
	case CTRL_CLOSE_EVENT:
		return TRUE;
	case CTRL_LOGOFF_EVENT:
		return TRUE;
	case CTRL_SHUTDOWN_EVENT:
		return TRUE;
	}
	return FALSE;
}
void Loading()
{
	SetConsoleCtrlHandler(CtrlHandler, TRUE);
	ReadKeyCodes();
}

HDC hdcOrigin{nullptr}, hdcBuffer{nullptr};
HFONT hFontText{nullptr};
PAINTSTRUCT ps;

inline HFONT CJZAPI CreateFont(int height, int width, LPCSTR lpFamilyName)
{
	return CreateFont(height,width,0,0,FW_NORMAL,0,0,0,0,0,0,0,0,lpFamilyName);
}
void BeginDraw()
{
	hdcOrigin = BeginPaint(hwnd, &ps);
	hFontText = CreateFontA(TIP_FS, 0, TIP_FONTNAME);
	SelectObject(hdcBuffer, hFontText);
	SetBkMode(hdcBuffer, TRANSPARENT);
}
void EndDraw()
{
	DeleteObject(hFontText);
	EndPaint(hwnd, &ps);
}
void ClearDevice(HDC _hdc = hdcBuffer, HWND _hwnd = hwnd)
{
	// 清屏：使用透明色填充整个客户区域
	RECT rcClient;
	GetClientRect(_hwnd, &rcClient);
	HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
	FillRect(_hdc, &rcClient, hBrush);
	DeleteObject(hBrush);
}

void DrawTips()
{
	RECT rt {.left = 0, .top = 1, .right = scr_w, .bottom = tipwnd_h};
	
	if(g_ui_stage == UIRecording || g_ui_stage == UIExecuting || g_ui_stage == UIDisplaying)
	{
		POINT pt;
		GetCursorPos(&pt);
		string s = sprintf2("(%ld, %ld)  ", pt.x, pt.y);
		for(auto const& e : eventMgr.recent_events)
		{
			s += e.ToDisplayString();
			s += "  ";
		}
		SetTextColor(hdcBuffer, RainbowColor());
		DrawTextA(hdcBuffer, s.c_str(), s.length(), &rt, DT_CENTER);
	}else if(g_ui_stage == UIExecutePre)
	{
		string s{"--- Ready To Execute ---"};
		SetTextColor(hdcBuffer, RainbowColorQuick());
		DrawTextA(hdcBuffer, s.c_str(), s.length(), &rt, DT_CENTER);
	}else if(g_ui_stage == UIExecuteEnd)
	{
		string s{"--- Execution Ended ---"};
		SetTextColor(hdcBuffer, RainbowColorQuick());
		DrawTextA(hdcBuffer, s.c_str(), s.length(), &rt, DT_CENTER);
	}else if(g_ui_stage == UIRecordPre)
	{
		string s{"--- Ready To Record ---"};
		SetTextColor(hdcBuffer, RainbowColorQuick());
		DrawTextA(hdcBuffer, s.c_str(), s.length(), &rt, DT_CENTER);
	}else if(g_ui_stage == UIRecordPause)
	{
		string s{"已暂停录制"};
		SetTextColor(hdcBuffer, RainbowColorQuick());
		DrawTextA(hdcBuffer, s.c_str(), s.length(), &rt, DT_CENTER);
	}else if(g_ui_stage == UIRecordEnd)
	{
		string s{"--- Record Completed ---"};
		SetTextColor(hdcBuffer, RainbowColorQuick());
		DrawTextA(hdcBuffer, s.c_str(), s.length(), &rt, DT_CENTER);
	}
	
}

#define TIMER_PAINT_CD 10L
VOID CALLBACK TimerProc_Paint(
	_In_  HWND hwnd,
	_In_  UINT uMsg,
	_In_  UINT_PTR idEvent,
	_In_  DWORD dwTime
	)
{
	RECT rect;
	GetClientRect(hwnd,&rect);
	InvalidateRect(hwnd, &rect, FALSE);	//会发送WM_PAINT消息
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) 
{
	static HBITMAP hBitmap = NULL;
	
	switch(Message) 
	{
		case WM_CREATE:{
			hdcBuffer = CreateCompatibleDC(NULL);
			SetTimer(hwnd, 0, TIMER_PAINT_CD, TimerProc_Paint);
			break;
		}
		case WM_DESTROY: {
			if (hdcBuffer)
				DeleteDC(hdcBuffer), hdcBuffer = nullptr;
			if (hBitmap)
				DeleteObject(hBitmap), hBitmap = nullptr;
			KillTimer(hwnd, 0);
			PostQuitMessage(0);
			break;
		}
		case WM_PAINT:{
			BeginDraw();
			
			// 获取客户区域的大小
			RECT rcClient;
			GetClientRect(hwnd, &rcClient);
			int clientWidth = rcClient.right - rcClient.left;
			int clientHeight = rcClient.bottom - rcClient.top;
			
			// 创建双缓冲
			if (hBitmap)
				DeleteObject(hBitmap);
			hBitmap = CreateCompatibleBitmap(hdcOrigin, clientWidth, clientHeight);
			SelectObject(hdcBuffer, hBitmap);
			
			ClearDevice();
			DrawTips();
			
			// 将缓冲区的内容一次性绘制到屏幕上
			BitBlt(hdcOrigin, 0, 0, clientWidth, clientHeight, hdcBuffer, 0, 0, SRCCOPY);
			EndDraw();
			break;
		}
	default:
		return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}
bool TerminalCheck(DWORD dwPid, HWND _hwnd)
{	//检查是否为win11新终端
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);	
	if (INVALID_HANDLE_VALUE == hSnapshot) 	
	{		
		return false;	
	}	
	PROCESSENTRY32 pe = { sizeof(pe) };	
	BOOL fOk;	
	for (fOk = Process32First(hSnapshot, &pe); fOk; fOk = Process32Next(hSnapshot, &pe)) 	
	{		
		if (!stricmp(pe.szExeFile, "WindowsTerminal.exe")
			&& pe.th32ProcessID == dwPid) 		
		{			
			CloseHandle(hSnapshot);			
			char title[MAX_PATH]{0};
			GetWindowText(_hwnd, title, MAX_PATH);
			if(strcmp(title, _pgmptr) && strcmp(title, "UserStimulator"))
				return false;
			return true;
		}	
	}	
	return false;
}
BOOL CALLBACK EnumWindowsProc(HWND _hwnd, LPARAM lParam)
{
	DWORD pid;
	GetWindowThreadProcessId(_hwnd, &pid);
	if(TerminalCheck(pid, _hwnd))
	{
		hwnd_console = _hwnd;
		return FALSE;
	}
	return TRUE;
}
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	g_ui_stage = UILoad;
	g_admin = IsRunAsAdmin();
	
	_hInstance = hInstance;
	_hPrevInstance = hPrevInstance;
	_lpCmdLine = lpCmdLine;
	_nShowCmd = nShowCmd;
	
	scr_w = GetScreenWidth();
	scr_h = GetScreenHeight();
	taskbar_h = GetTaskbarHeight();
	
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = USWND_CLASS_NAME;
	
	if(!RegisterClass(&wc)) {
		MessageBox(NULL, "提示窗口类注册失败","UserStimulator",MB_ICONERROR|MB_OK);
		return 0;
	}
	
	hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST, 
		USWND_CLASS_NAME, USWND_CAPTION, WS_POPUP,
		0, /* x */
		TIPWND_Y, /* y */
		scr_w, /* width */
		tipwnd_h, /* height */
		//GetTaskbarWindow(),
				NULL,
		NULL,hInstance,NULL);
	
	if(hwnd == NULL) {
		MessageBox(NULL, "提示窗口创建失败","UserStimulator",MB_ICONERROR|MB_OK);
		return 0;
	}
	SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 200, LWA_COLORKEY | LWA_ALPHA);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	TipWindowSwitch(false);
	
	SetConsoleTitleA("UserStimulator");
	if(ExistProcess("WindowsTerminal.exe"))
	{	//win11电脑且使用新版终端
		EnumWindows(EnumWindowsProc, 0);
	}else{	//旧版控制台主机
		hwnd_console = GetConsoleWindow();
	}
	if(!hwnd_console || hwnd_console == INVALID_HANDLE_VALUE)
	{
		hwnd_console = GetForegroundWindow();
	}
	
	Loading();
//	getch();
	EnterMainUI();
	SendMessage(hwnd, WM_DESTROY, 0, 0);
	FreeConsole();
	Sleep(500);
	return 0;
}
