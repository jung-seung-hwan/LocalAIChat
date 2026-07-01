#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma execution_character_set("utf-8")

struct ChatMessage
{
    std::string role;
    std::string content;
};

const std::string ChatHistoryFileName = "chat_history.txt";

std::string EscapeHistoryContent(const std::string& content)
{
    std::string escaped;

    for (char ch : content)
    {
        if (ch == '\\')
        {
            escaped += "\\\\";
        }
        else if (ch == '\n')
        {
            escaped += "\\n";
        }
        else if (ch == '\r')
        {
            escaped += "\\r";
        }
        else if (ch == '\t')
        {
            escaped += "\\t";
        }
        else
        {
            escaped += ch;
        }
    }

    return escaped;
}

std::string RestoreHistoryContent(const std::string& content)
{
    std::string restored;

    for (size_t i = 0; i < content.size(); i++)
    {
        if (content[i] != '\\' || i + 1 >= content.size())
        {
            restored += content[i];
            continue;
        }

        char next = content[i + 1];

        if (next == '\\')
        {
            restored += '\\';
        }
        else if (next == 'n')
        {
            restored += '\n';
        }
        else if (next == 'r')
        {
            restored += '\r';
        }
        else if (next == 't')
        {
            restored += '\t';
        }
        else
        {
            restored += next;
        }

        i++;
    }

    return restored;
}

bool SaveChatHistoryToFile(const std::vector<ChatMessage>& chatHistory)
{
    std::ofstream outputFile(ChatHistoryFileName);

    if (!outputFile.is_open())
    {
        return false;
    }

    for (const ChatMessage& message : chatHistory)
    {
        outputFile << message.role << '\t' << EscapeHistoryContent(message.content) << '\n';
    }

    return true;
}

bool LoadChatHistoryFromFile(std::vector<ChatMessage>& chatHistory)
{
    DWORD fileAttributes = GetFileAttributesA(ChatHistoryFileName.c_str());

    if (fileAttributes == INVALID_FILE_ATTRIBUTES)
    {
        DWORD errorCode = GetLastError();

        if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND)
        {
            return true;
        }

        return false;
    }

    std::ifstream inputFile(ChatHistoryFileName);

    if (!inputFile.is_open())
    {
        return false;
    }

    std::string line;

    while (std::getline(inputFile, line))
    {
        size_t tabPosition = line.find('\t');

        if (tabPosition == std::string::npos)
        {
            std::cout << "형식이 맞지 않는 대화 기록 한 줄을 건너뜁니다: " << line << std::endl;
            continue;
        }

        ChatMessage message;
        message.role = line.substr(0, tabPosition);
        message.content = RestoreHistoryContent(line.substr(tabPosition + 1));

        chatHistory.push_back(message);
    }

    return true;
}

void PrintChatHistory(const std::vector<ChatMessage>& chatHistory)
{
    if (chatHistory.empty())
    {
        std::cout << "저장된 대화 기록이 없습니다." << std::endl;
        return;
    }

    std::cout << "현재 메모리에 저장된 대화 기록" << std::endl;

    for (size_t i = 0; i < chatHistory.size(); i++)
    {
        const ChatMessage& message = chatHistory[i];

        if (message.role == "user")
        {
            std::cout << i + 1 << ". You: " << message.content << std::endl;
        }
        else if (message.role == "assistant")
        {
            std::cout << i + 1 << ". AI: " << message.content << std::endl;
        }
        else
        {
            std::cout << i + 1 << ". " << message.role << ": " << message.content << std::endl;
        }
    }
}

void PrintHelp()
{
    std::cout << "사용 가능한 명령어" << std::endl;
    std::cout << "/help    : 사용 가능한 명령어를 보여줍니다." << std::endl;
    std::cout << "/history : 현재 메모리에 저장된 대화 기록을 보여줍니다." << std::endl;
    std::cout << "/clear   : 현재 메모리에 저장된 대화 기록을 모두 삭제합니다." << std::endl;
    std::cout << "/exit    : 프로그램을 종료합니다." << std::endl;
}

void ClearChatHistory(std::vector<ChatMessage>& chatHistory)
{
    chatHistory.clear();

    if (SaveChatHistoryToFile(chatHistory))
    {
        std::cout << "대화 기록을 모두 삭제했습니다." << std::endl;
    }
    else
    {
        std::cout << "메모리 대화 기록은 삭제했지만, 파일을 비우지 못했습니다." << std::endl;
    }
}

void RemoveLeadingUtf8Bom(std::string& text)
{
    const std::string utf8Bom = "\xEF\xBB\xBF";

    if (text.rfind(utf8Bom, 0) == 0)
    {
        text.erase(0, utf8Bom.size());
    }
}

std::string Trim(const std::string& text)
{
    size_t start = 0;

    while (start < text.size())
    {
        char ch = text[start];

        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
        {
            break;
        }

        start++;
    }

    if (start == text.size())
    {
        return "";
    }

    size_t end = text.size() - 1;

    while (end > start)
    {
        char ch = text[end];

        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
        {
            break;
        }

        end--;
    }

    return text.substr(start, end - start + 1);
}

std::string EscapeJsonString(const std::string& text)
{
    std::ostringstream escaped;

    for (unsigned char ch : text)
    {
        if (ch == '\"')
        {
            escaped << "\\\"";
        }
        else if (ch == '\\')
        {
            escaped << "\\\\";
        }
        else if (ch == '\n')
        {
            escaped << "\\n";
        }
        else if (ch == '\r')
        {
            escaped << "\\r";
        }
        else if (ch == '\t')
        {
            escaped << "\\t";
        }
        else if (ch < 0x20)
        {
            escaped << "\\u"
                    << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(ch)
                    << std::dec;
        }
        else
        {
            escaped << ch;
        }
    }

    return escaped.str();
}

std::string BuildRequestBody(const std::string& userInput)
{
    return std::string("{")
        + "\"model\":\"qwen3:0.6b\","
        + "\"messages\":[{\"role\":\"user\",\"content\":\""
        + EscapeJsonString(userInput)
        + "\"}],"
        + "\"stream\":false"
        + "}";
}

void SkipWhitespace(const std::string& text, size_t& position)
{
    while (position < text.size())
    {
        char ch = text[position];

        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t')
        {
            break;
        }

        position++;
    }
}

int HexDigitToInt(char ch)
{
    if ('0' <= ch && ch <= '9')
    {
        return ch - '0';
    }

    if ('a' <= ch && ch <= 'f')
    {
        return ch - 'a' + 10;
    }

    if ('A' <= ch && ch <= 'F')
    {
        return ch - 'A' + 10;
    }

    return -1;
}

bool ReadJsonUnicodeCodePoint(const std::string& json, size_t position, unsigned int& codePoint)
{
    if (position + 4 > json.size())
    {
        return false;
    }

    codePoint = 0;

    for (int i = 0; i < 4; i++)
    {
        int value = HexDigitToInt(json[position + i]);

        if (value == -1)
        {
            return false;
        }

        codePoint = codePoint * 16 + static_cast<unsigned int>(value);
    }

    return true;
}

void AppendUtf8(std::string& text, unsigned int codePoint)
{
    if (codePoint <= 0x7F)
    {
        text += static_cast<char>(codePoint);
    }
    else if (codePoint <= 0x7FF)
    {
        text += static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
        text += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0xFFFF)
    {
        text += static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
        text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0x10FFFF)
    {
        text += static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07));
        text += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
}

bool DecodeJsonString(const std::string& json, size_t quotePosition, std::string& value, size_t& nextPosition)
{
    if (quotePosition >= json.size() || json[quotePosition] != '\"')
    {
        return false;
    }

    value = "";
    size_t position = quotePosition + 1;

    while (position < json.size())
    {
        char ch = json[position];

        if (ch == '\"')
        {
            nextPosition = position + 1;
            return true;
        }

        if (ch != '\\')
        {
            value += ch;
            position++;
            continue;
        }

        position++;

        if (position >= json.size())
        {
            return false;
        }

        char escaped = json[position];

        if (escaped == '\"')
        {
            value += '\"';
        }
        else if (escaped == '\\')
        {
            value += '\\';
        }
        else if (escaped == '/')
        {
            value += '/';
        }
        else if (escaped == 'b')
        {
            value += '\b';
        }
        else if (escaped == 'f')
        {
            value += '\f';
        }
        else if (escaped == 'n')
        {
            value += '\n';
        }
        else if (escaped == 'r')
        {
            value += '\r';
        }
        else if (escaped == 't')
        {
            value += '\t';
        }
        else if (escaped == 'u')
        {
            unsigned int codePoint = 0;

            if (!ReadJsonUnicodeCodePoint(json, position + 1, codePoint))
            {
                return false;
            }

            position += 4;

            // 이모지처럼 두 개의 \u 값으로 표현되는 문자를 처리합니다.
            if (0xD800 <= codePoint && codePoint <= 0xDBFF)
            {
                if (position + 6 < json.size() && json[position + 1] == '\\' && json[position + 2] == 'u')
                {
                    unsigned int lowSurrogate = 0;

                    if (ReadJsonUnicodeCodePoint(json, position + 3, lowSurrogate)
                        && 0xDC00 <= lowSurrogate && lowSurrogate <= 0xDFFF)
                    {
                        codePoint = 0x10000
                            + ((codePoint - 0xD800) * 0x400)
                            + (lowSurrogate - 0xDC00);
                        position += 6;
                    }
                }
            }

            AppendUtf8(value, codePoint);
        }
        else
        {
            return false;
        }

        position++;
    }

    return false;
}

bool ExtractMessageContent(const std::string& json, std::string& messageContent)
{
    messageContent = "";

    size_t messagePosition = json.find("\"message\"");

    if (messagePosition == std::string::npos)
    {
        return false;
    }

    size_t messageColon = json.find(':', messagePosition);

    if (messageColon == std::string::npos)
    {
        return false;
    }

    size_t objectStart = json.find('{', messageColon);

    if (objectStart == std::string::npos)
    {
        return false;
    }

    size_t contentPosition = json.find("\"content\"", objectStart);

    if (contentPosition == std::string::npos)
    {
        return false;
    }

    size_t contentColon = json.find(':', contentPosition);

    if (contentColon == std::string::npos)
    {
        return false;
    }

    size_t valueStart = contentColon + 1;
    SkipWhitespace(json, valueStart);

    size_t nextPosition = 0;
    return DecodeJsonString(json, valueStart, messageContent, nextPosition);
}

bool ExtractJsonStringField(const std::string& json, const std::string& fieldName, std::string& fieldValue)
{
    fieldValue = "";

    std::string key = "\"" + fieldName + "\"";
    size_t keyPosition = json.find(key);

    if (keyPosition == std::string::npos)
    {
        return false;
    }

    size_t colonPosition = json.find(':', keyPosition);

    if (colonPosition == std::string::npos)
    {
        return false;
    }

    size_t valueStart = colonPosition + 1;
    SkipWhitespace(json, valueStart);

    size_t nextPosition = 0;
    return DecodeJsonString(json, valueStart, fieldValue, nextPosition);
}

std::string BuildWinHttpErrorMessage(const std::string& action, DWORD errorCode)
{
    std::string message = action + " 실패. 오류 코드: " + std::to_string(errorCode);

    if (errorCode == ERROR_WINHTTP_CANNOT_CONNECT
        || errorCode == ERROR_WINHTTP_CONNECTION_ERROR
        || errorCode == ERROR_WINHTTP_TIMEOUT)
    {
        message += "\nOllama 서버가 꺼져 있거나 localhost:11434에 연결할 수 없습니다.";
        message += "\n확인: PowerShell에서 ollama run qwen3:0.6b 를 실행했는지 확인하세요.";
    }

    return message;
}

std::string BuildOllamaApiErrorMessage(DWORD statusCode, const std::string& response)
{
    std::string message = "Ollama API가 HTTP 상태 코드 "
        + std::to_string(statusCode)
        + " 를 반환했습니다.";

    std::string ollamaError;

    if (ExtractJsonStringField(response, "error", ollamaError))
    {
        message += "\nOllama 오류 메시지: " + ollamaError;
        message += "\n모델 이름이 qwen3:0.6b인지, 해당 모델이 설치되어 있는지 확인하세요.";
    }
    else if (!response.empty())
    {
        message += "\n응답 본문: " + response;
    }
    else
    {
        message += "\n응답 본문이 비어 있습니다.";
    }

    return message;
}

void CloseWinHttpHandle(HINTERNET handle)
{
    if (handle != NULL)
    {
        WinHttpCloseHandle(handle);
    }
}

bool SendMessageToOllama(const std::string& userInput, std::string& response, std::string& errorMessage)
{
    response = "";
    errorMessage = "";

    std::string requestBody = BuildRequestBody(userInput);

    HINTERNET session = WinHttpOpen(
        L"LocalAIChat/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (session == NULL)
    {
        errorMessage = BuildWinHttpErrorMessage("WinHttpOpen", GetLastError());
        return false;
    }

    HINTERNET connection = WinHttpConnect(session, L"localhost", 11434, 0);

    if (connection == NULL)
    {
        errorMessage = BuildWinHttpErrorMessage("WinHttpConnect", GetLastError());
        CloseWinHttpHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(
        connection,
        L"POST",
        L"/api/chat",
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0
    );

    if (request == NULL)
    {
        errorMessage = BuildWinHttpErrorMessage("WinHttpOpenRequest", GetLastError());
        CloseWinHttpHandle(connection);
        CloseWinHttpHandle(session);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n";

    BOOL sendResult = WinHttpSendRequest(
        request,
        headers.c_str(),
        static_cast<DWORD>(-1L),
        const_cast<char*>(requestBody.c_str()),
        static_cast<DWORD>(requestBody.size()),
        static_cast<DWORD>(requestBody.size()),
        0
    );

    if (sendResult == FALSE)
    {
        errorMessage = BuildWinHttpErrorMessage("WinHttpSendRequest", GetLastError());
        CloseWinHttpHandle(request);
        CloseWinHttpHandle(connection);
        CloseWinHttpHandle(session);
        return false;
    }

    BOOL receiveResult = WinHttpReceiveResponse(request, NULL);

    if (receiveResult == FALSE)
    {
        errorMessage = BuildWinHttpErrorMessage("WinHttpReceiveResponse", GetLastError());
        CloseWinHttpHandle(request);
        CloseWinHttpHandle(connection);
        CloseWinHttpHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);

    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX
    );

    DWORD bytesAvailable = 0;

    do
    {
        bytesAvailable = 0;

        if (WinHttpQueryDataAvailable(request, &bytesAvailable) == FALSE)
        {
            errorMessage = BuildWinHttpErrorMessage("WinHttpQueryDataAvailable", GetLastError());
            CloseWinHttpHandle(request);
            CloseWinHttpHandle(connection);
            CloseWinHttpHandle(session);
            return false;
        }

        if (bytesAvailable > 0)
        {
            std::string buffer(bytesAvailable, '\0');
            DWORD bytesRead = 0;

            if (WinHttpReadData(request, &buffer[0], bytesAvailable, &bytesRead) == FALSE)
            {
                errorMessage = BuildWinHttpErrorMessage("WinHttpReadData", GetLastError());
                CloseWinHttpHandle(request);
                CloseWinHttpHandle(connection);
                CloseWinHttpHandle(session);
                return false;
            }

            buffer.resize(bytesRead);
            response += buffer;
        }
    } while (bytesAvailable > 0);

    CloseWinHttpHandle(request);
    CloseWinHttpHandle(connection);
    CloseWinHttpHandle(session);

    if (statusCode != 200)
    {
        errorMessage = BuildOllamaApiErrorMessage(statusCode, response);
        return false;
    }

    return true;
}

void LoadInitialChatHistory(std::vector<ChatMessage>& chatHistory)
{
    if (LoadChatHistoryFromFile(chatHistory))
    {
        if (!chatHistory.empty())
        {
            std::cout << "이전 대화 기록 " << chatHistory.size() << "개를 불러왔습니다." << std::endl;
        }
    }
    else
    {
        std::cout << "대화 기록 파일을 불러오지 못했습니다. 빈 기록으로 시작합니다." << std::endl;
    }
}

void PrintStartupMessage()
{
    std::cout << "Local AI Chat - Step 9" << std::endl;
    std::cout << "Ollama 로컬 LLM API와 연결하는 콘솔 프로그램입니다." << std::endl;
    std::cout << "실행 전에 Ollama와 qwen3:0.6b 모델이 켜져 있어야 합니다." << std::endl;
    std::cout << "종료하려면 /exit 을 입력하세요." << std::endl;
    std::cout << "명령어 목록을 보려면 /help 를 입력하세요." << std::endl;
}

std::string ReadUserInput()
{
    std::string userInput;

    std::cout << std::endl;
    std::cout << "입력: ";

    std::getline(std::cin, userInput);
    RemoveLeadingUtf8Bom(userInput);

    return Trim(userInput);
}

bool HandleCommand(const std::string& userInput, std::vector<ChatMessage>& chatHistory, bool& shouldExit)
{
    shouldExit = false;

    if (userInput == "/exit")
    {
        std::cout << "프로그램을 종료합니다." << std::endl;
        shouldExit = true;
        return true;
    }

    if (userInput == "/help")
    {
        PrintHelp();
        return true;
    }

    if (userInput == "/history")
    {
        PrintChatHistory(chatHistory);
        return true;
    }

    if (userInput == "/clear")
    {
        ClearChatHistory(chatHistory);
        return true;
    }

    if (userInput[0] == '/')
    {
        std::cout << "알 수 없는 명령어입니다. /help 를 입력해 사용 가능한 명령어를 확인하세요." << std::endl;
        return true;
    }

    return false;
}

void SaveSuccessfulConversation(
    std::vector<ChatMessage>& chatHistory,
    const std::string& userInput,
    const std::string& messageContent)
{
    ChatMessage userMessage;
    userMessage.role = "user";
    userMessage.content = userInput;

    ChatMessage assistantMessage;
    assistantMessage.role = "assistant";
    assistantMessage.content = messageContent;

    chatHistory.push_back(userMessage);
    chatHistory.push_back(assistantMessage);

    if (!SaveChatHistoryToFile(chatHistory))
    {
        std::cout << "주의: 대화 기록을 파일에 저장하지 못했습니다." << std::endl;
    }
}

void HandleChatMessage(const std::string& userInput, std::vector<ChatMessage>& chatHistory)
{
    std::cout << "You: " << userInput << std::endl;
    std::cout << "AI 응답 요청 중..." << std::endl;

    std::string response;
    std::string errorMessage;

    bool success = SendMessageToOllama(userInput, response, errorMessage);

    if (!success)
    {
        std::cout << "오류: " << errorMessage << std::endl;
        std::cout << "Ollama가 실행 중인지, qwen3:0.6b 모델이 준비되어 있는지 확인하세요." << std::endl;
        return;
    }

    std::string messageContent;

    if (ExtractMessageContent(response, messageContent))
    {
        std::cout << "AI: " << messageContent << std::endl;
        SaveSuccessfulConversation(chatHistory, userInput, messageContent);
        return;
    }

    std::cout << "오류: message.content 값을 찾지 못했습니다." << std::endl;
    std::cout << "디버깅용 전체 JSON: " << response << std::endl;
}

void RunChatLoop(std::vector<ChatMessage>& chatHistory)
{
    while (true)
    {
        std::string userInput = ReadUserInput();

        if (userInput == "")
        {
            std::cout << "빈 입력이거나 공백만 입력되었습니다. 다시 입력해주세요." << std::endl;
            continue;
        }

        bool shouldExit = false;

        if (HandleCommand(userInput, chatHistory, shouldExit))
        {
            if (shouldExit)
            {
                break;
            }

            continue;
        }

        HandleChatMessage(userInput, chatHistory);
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::vector<ChatMessage> chatHistory;

    LoadInitialChatHistory(chatHistory);
    PrintStartupMessage();
    RunChatLoop(chatHistory);

    return 0;
}
