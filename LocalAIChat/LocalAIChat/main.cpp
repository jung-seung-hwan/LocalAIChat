#include <windows.h>
#include <winhttp.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")
#pragma execution_character_set("utf-8")

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
        errorMessage = "WinHttpOpen 실패. 오류 코드: " + std::to_string(GetLastError());
        return false;
    }

    HINTERNET connection = WinHttpConnect(session, L"localhost", 11434, 0);

    if (connection == NULL)
    {
        errorMessage = "WinHttpConnect 실패. Ollama가 실행 중인지 확인하세요. 오류 코드: " + std::to_string(GetLastError());
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
        errorMessage = "WinHttpOpenRequest 실패. 오류 코드: " + std::to_string(GetLastError());
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
        errorMessage = "WinHttpSendRequest 실패. Ollama API 요청을 보낼 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
        CloseWinHttpHandle(request);
        CloseWinHttpHandle(connection);
        CloseWinHttpHandle(session);
        return false;
    }

    BOOL receiveResult = WinHttpReceiveResponse(request, NULL);

    if (receiveResult == FALSE)
    {
        errorMessage = "WinHttpReceiveResponse 실패. Ollama 응답을 받을 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
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
            errorMessage = "WinHttpQueryDataAvailable 실패. 응답 크기를 확인할 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
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
                errorMessage = "WinHttpReadData 실패. 응답을 읽을 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
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
        errorMessage = "Ollama API가 HTTP 상태 코드 " + std::to_string(statusCode) + " 를 반환했습니다.";
        return false;
    }

    return true;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::string userInput;

    std::cout << "Local AI Chat - Step 3" << std::endl;
    std::cout << "Ollama 로컬 LLM API와 연결하는 콘솔 프로그램입니다." << std::endl;
    std::cout << "실행 전에 Ollama와 qwen3:0.6b 모델이 켜져 있어야 합니다." << std::endl;
    std::cout << "종료하려면 /exit 을 입력하세요." << std::endl;

    while (true)
    {
        std::cout << std::endl;
        std::cout << "입력: ";

        std::getline(std::cin, userInput);

        if (userInput == "")
        {
            std::cout << "빈 입력입니다. 다시 입력해주세요." << std::endl;
            continue;
        }

        if (userInput == "/exit")
        {
            std::cout << "프로그램을 종료합니다." << std::endl;
            break;
        }

        std::cout << "You: " << userInput << std::endl;
        std::cout << "AI 응답 요청 중..." << std::endl;

        std::string response;
        std::string errorMessage;

        bool success = SendMessageToOllama(userInput, response, errorMessage);

        if (success)
        {
            std::cout << "AI 응답(JSON 전체): " << response << std::endl;
        }
        else
        {
            std::cout << "오류: " << errorMessage << std::endl;
            std::cout << "Ollama가 실행 중인지, qwen3:0.6b 모델이 준비되어 있는지 확인하세요." << std::endl;
        }
    }

    return 0;
}
