#include "../../manager/manager/Manager.hpp"
#include <future>

using namespace std::chrono_literals;

HANDLE commPipeCl;

void pipeConnect() {
    commPipeCl = CreateFile(
        L"\\\\.\\pipe\\commPipe",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (commPipeCl == INVALID_HANDLE_VALUE)
    {
        std::osyncstream(std::cout) << "Failed to connect to commPipe\n";
        return;
    }
}

void pipeCreate() {
    commPipeCl = CreateNamedPipe(
        L"\\\\.\\pipe\\commPipe", 
        PIPE_ACCESS_DUPLEX, 
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 
        1, 
        1024 * 16, 
        1024 * 16, 
        NMPWAIT_USE_DEFAULT_WAIT, 
        NULL
    );  
    if (commPipeCl == NULL || commPipeCl == INVALID_HANDLE_VALUE) {
        std::osyncstream(std::cout) << "Failed to create outbound pipe instance(commPipe)\n";
        return;
    }
    BOOL result = ConnectNamedPipe(commPipeCl, NULL);
    if (!result)
    {
        std::osyncstream(std::cout) << "Failed to make connection on named pipe(commPipe)\n";
        CloseHandle(commPipeCl);
        return;
    }
    Sleep(100);
}

void writeToPipe(bool status) {
    DWORD numBytesWritten = 0;
    BOOL result = WriteFile(commPipeCl, &status, sizeof(status), &numBytesWritten, NULL);
    if (!result)
    {
        //std::osyncstream(std::cout) << "Failed to send data\n";
        return;
    }
}

bool readFromPipe() {
    DWORD numBytesRead1 = 0;
    bool status;
    BOOL result = ReadFile(commPipeCl, &status, sizeof(status), &numBytesRead1, NULL);
    if (result)
    {
        return status;
    }
    else
    {
        //if we can't get info from another process
        return false;
    }
}


class Client {
public: 
    static bool isReady;
    static char cancellationOption;

    template<typename T>
    void passResByPipe(ResultAndAttempts<T> compResult, std::string functionParam) 
    {
        DWORD numBytesWritten = 0;
        HANDLE myPipe;

        if (functionParam == "f") 
        {
            myPipe = CreateFile(
                L"\\\\.\\pipe\\functionFpipe",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
        }
        else 
        {
            myPipe = CreateFile(
                L"\\\\.\\pipe\\functionGpipe",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
        }

        if (myPipe == INVALID_HANDLE_VALUE) 
        {
            std::osyncstream(std::cout) << "Failed to connect to functionPipe\n";
            return;
        }
        BOOL result = WriteFile(myPipe, &compResult, sizeof(compResult), &numBytesWritten, NULL);
        if (!result) 
        {
            std::osyncstream(std::cout) << "Failed to send data(functionPipe)\n";
            return;
        }

        CloseHandle(myPipe);
    }

    template<typename T>
    void manageFunction(std::string functionParam, int numberParam, std::function<std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T>(int)> functionF,
        std::function<std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T>(int)> functionG)
    {
        std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T> compResult;
        clock_t startCountTime, endCountTime;
        ResultAndAttempts<T> resultAndAttempts;
        std::future<bool> getStatus;
        HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXT("myMutex"));
        if (hMutex == NULL) 
        {
            std::osyncstream(std::cout) << "OpenMutex error: " << GetLastError() << std::endl;
        }

        int tryCount = 0;
        bool toStopExecuting = false;
        for (int i = 0; i < 5; i++) 
        {
            if (toStopExecuting == true) 
            {
                break;
            }

            cancellationOption = NULL;
            isReady = false;
            tryCount++;
            std::future<bool> getStatus = std::async(std::launch::async, []() {
                while (isReady == false) 
                {
                    std::this_thread::sleep_for(0.01s);
                }
                return true;
            });

            if (functionParam == "f") 
            {
                std::osyncstream(std::cout) << "f(): works\n";
                std::thread funcFthread([this, functionF, numberParam, &compResult] {runFunction(functionF, numberParam, compResult); });
                funcFthread.detach();
            }
            else 
            {
                std::osyncstream(std::cout) << "g(): works\n";
                std::thread funcGthread([this, functionG, numberParam, &compResult] {runFunction(functionG, numberParam, compResult); });
                funcGthread.detach();
            }
            if (std::future_status::timeout==getStatus.wait_for(6s)) 
            {
                compResult = os::lab1::compfuncs::soft_fail();
            }

            switch (compResult.index()) 
            {
                case 0:
                    std::osyncstream(std::cout) << functionParam << "(): finish\n";
                    resultAndAttempts.result = compResult;
                    resultAndAttempts.attemtps = tryCount;
                    passResByPipe<T>(resultAndAttempts, functionParam);
                    return;
                case 1:
                    std::osyncstream(std::cout) << functionParam << "(): soft fail\n";
                    resultAndAttempts.result = compResult;
                    break;
                case 2:
                    std::osyncstream(std::cout) << functionParam << "(): finish\n";
                    resultAndAttempts.result = compResult;
                    resultAndAttempts.attemtps = tryCount;
                    passResByPipe<T>(resultAndAttempts, functionParam);
                    return;
            }

            DWORD mutex = WaitForSingleObject(hMutex, 0);
            if (std::holds_alternative<os::lab1::compfuncs::soft_fail>(compResult) && mutex == WAIT_OBJECT_0)
            {
                startCountTime = clock();
                std::thread cancellationThread([this,functionParam] {cancellationDialog(functionParam); });
                cancellationThread.detach();
                toStopExecuting = false;
                while (true)
                {
                    endCountTime = clock();
                    if (std::future_status::ready == getStatus.wait_for(0.01s))
                    {
                        toStopExecuting = true;
                        std::osyncstream(std::cout) << functionParam <<"(): finish\n";
                        writeToPipe(false);
                        break;
                    }
                    if (((double)(endCountTime - startCountTime) / CLOCKS_PER_SEC) > 5)
                    {
                        std::osyncstream(std::cout) << "Action is not confirmed within 5 seconds. proceeding..\n";
                        writeToPipe(false);
                        break;
                    }
                    if (cancellationOption == 'y' || cancellationOption == 'Y')
                    {
                        toStopExecuting = true;
                        tryCount = -tryCount;
                        writeToPipe(true);
                        break;
                    }
                    else if (cancellationOption == 'n' || cancellationOption == 'N')
                    {
                        writeToPipe(false);
                        break;
                    }
                    else if ((cancellationOption >= 32 && cancellationOption <= 126)) 
                    {
                        std::osyncstream(std::cout) << "Wrong option! Continue computation..\n";
                        writeToPipe(false);
                        break;
                    }
                }
                ReleaseMutex(hMutex);
            }
            else 
            {
                while (true)
                {
                    mutex = WaitForSingleObject(hMutex, 0);
                    if (mutex == WAIT_OBJECT_0) 
                    {
                        ReleaseMutex(hMutex);
                        break;
                    }
                }
                toStopExecuting = readFromPipe();
                if (toStopExecuting == true) 
                {
                    tryCount = -tryCount;
                }
            }
            //in order to cancel async thread
            isReady = true;
        }

        resultAndAttempts.result = compResult;
        resultAndAttempts.attemtps = tryCount;
        //isReady = true;
        CloseHandle(hMutex);
        passResByPipe<T>(resultAndAttempts, functionParam);
    }

    template<typename T>
    void runFunction(std::function<std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T>(int)> function, int x,
        std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T>& compResult) 
    {
        compResult = function(x);
        isReady = true;
    }

    void cancellationDialog(std::string functionParam)
    {
        std::osyncstream(std::cout) << functionParam + ": Please confirm that computation should be stopped y(es, stop) / n(ot yet)[n]\n";
        std::cin >> cancellationOption;
    }
};

bool Client::isReady = false;
char Client::cancellationOption = NULL;

int main(int argc, char* argv[])
{
    std::string functionParam(argv[0]);
    int numberParam = atoi(argv[1]);

    if (functionParam == "f") 
    {
        pipeCreate();
    }
    else 
    {
        Sleep(100);
        pipeConnect();
    }

    if (argc == 2) 
    {
        Client client;
        client.manageFunction<double>(functionParam,numberParam, os::lab1::compfuncs::trial_f<os::lab1::compfuncs::DOUBLE_MULT>, os::lab1::compfuncs::trial_g<os::lab1::compfuncs::DOUBLE_MULT>);
    }
    else 
    {
        std::osyncstream(std::cout) << "Unknown error in creating process\n";
    }
    CloseHandle(commPipeCl);
    return 0;
}




