#pragma once
#include "Widget.h"

using std::streambuf;

class UConsoleWidget;
class FLogFileWriter;
struct ImGuiInputTextCallbackData;

struct FLogEntry
{
	ELogType Type;
	FString Message;
};

/**
 * @brief Custom Stream Buffer
 * Redirects Output to ConsoleWidget
 */
class ConsoleStreamBuffer : public streambuf
{
public:
	ConsoleStreamBuffer(UConsoleWidget* InConsole, bool bInIsError = false)
		: Console(InConsole), bIsError(bInIsError)
	{
	}

protected:
	// Stream Buffer 함수 Override
	int overflow(int InCharacter) override;
	streamsize xsputn(const char* InString, streamsize InCount) override;

private:
	UConsoleWidget* Console;
	FString Buffer;
	bool bIsError;
};

/**
 * @brief Console Widget
 * 콘솔의 실제 기능을 담당하는 위젯 (로그 표시, 명령어 처리 등)
 */
UCLASS()
class UConsoleWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UConsoleWidget, UWidget)

public:
	// Widget interface
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	// Log functions
	void AddLog(const char* fmt, ...);
	void AddLog(ELogType InType, const char* fmt, ...);
	void AddSystemLog(const char* InText, bool bInIsError = false);
	void ClearLog();

	// Console command
	void ProcessCommand(const char* InCommand);
	void HandleStatCommand(const FString& StatCommand);
	void ExecuteTerminalCommand(const char* InCommand);

	// Use external terminal
	void InitializeSystemRedirect();
	void CleanupSystemRedirect();

	// History navigation
	int HandleHistoryCallback(ImGuiInputTextCallbackData* InData);
	bool IsSingleton() const override { return true; }

	// Console visibility tracking
	void OnConsoleShown();
	void ClearSelection();

private:
	// Command input
	char InputBuf[256];
	TArray<FString> CommandHistory;
	int HistoryPosition;

	// Log output
	std::deque<FLogEntry> LogItems;
	bool bIsAutoScroll;
	bool bIsScrollToBottom;
	bool bPendingScrollToBottom; // 콘솔이 열릴 때 스크롤을 하단으로 이동

	// Text selection for drag-copy
	struct FTextSelection
	{
		int StartLine = -1;
		int StartChar = -1;
		int EndLine = -1;
		int EndChar = -1;
		bool IsActive() const { return StartLine >= 0; }
		void Clear() { StartLine = StartChar = EndLine = EndChar = -1; }
	};
	FTextSelection TextSelection;
	bool bIsDragging = false;
	ImVec2 DragStartPos;

	// Stream redirection
	ConsoleStreamBuffer* ConsoleOutputBuffer;
	ConsoleStreamBuffer* ConsoleErrorBuffer;
	streambuf* OriginalConsoleOutput;
	streambuf* OriginalConsoleError;

	// Log file writer
	FLogFileWriter* LogFileWriter;

	// 초기화 전 로그 임시 버퍼
	std::deque<FLogEntry> PendingLogs;

	// Helper functions
	static ImVec4 GetColorByLogType(ELogType InType);
	static const char* GetLogTypePrefix(ELogType InType);

	void AddLogInternal(ELogType InType, const char* fmt, va_list InArguments);
};
