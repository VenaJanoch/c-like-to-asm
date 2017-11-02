#pragma once

class Compiler
{
public:
    Compiler();
    ~Compiler();

    int OnRun(int argc, wchar_t *argv[]);

    void OnError(const char* s);

private:

};
