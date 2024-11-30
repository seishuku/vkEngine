#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stdint.h>
#include <stdbool.h>

#define CONSOLE_MAX_NAME 32
#define CONSOLE_MAX_COMMANDS 64
#define CONSOLE_MAX_VARIABLES 64
#define CONSOLE_MAX_HISTORY 16
#define CONSOLE_MAX_LINES 256
#define CONSOLE_LINE_LENGTH 128

typedef struct Console_s Console_t;

typedef void (*ConsoleCommandFunc)(Console_t *console, const char *args);

typedef struct
{
    char name[CONSOLE_MAX_NAME];
    ConsoleCommandFunc func;
} ConsoleCommand_t;

typedef struct
{
    char name[CONSOLE_MAX_NAME];
    float value;
} ConsoleVariable_t;

typedef struct
{
    char buffer[CONSOLE_LINE_LENGTH];
} ConsoleLine_t;

typedef struct Console_s
{
    bool active;

    uint32_t numCommand;
    ConsoleCommand_t commands[CONSOLE_MAX_COMMANDS];

    uint32_t numVariable;
    ConsoleVariable_t variables[CONSOLE_MAX_VARIABLES];

    uint32_t numHistory, historyIndex;
    char history[CONSOLE_MAX_HISTORY][CONSOLE_LINE_LENGTH];

    uint32_t numLine;
    ConsoleLine_t lines[CONSOLE_MAX_LINES];

    uint32_t inputLength;
    char input[CONSOLE_LINE_LENGTH];

    uint32_t scrollbackOffset;
} Console_t;

void ConsoleInit(Console_t *console);
void ConsoleClear(Console_t *console, const char *param);
void ConsoleRegisterCommand(Console_t *console, const char *name, ConsoleCommandFunc func);
void ConsoleRegisterVariable(Console_t *console, const char *name, float value);
void ConsoleSetVariable(Console_t *console, const char *name, float value);
float ConsoleGetVariable(Console_t *console, const char *name);
void ConsolePrint(Console_t *console, const char *text);
void ConsolePrintCommands(Console_t *console, const char *param);
void ConsolePrintVariables(Console_t *console, const char *param);
void ConsoleScroll(Console_t *console, const bool up);
void ConsoleExecuteCommand(Console_t *console, const char *input);
void ConsoleHistory(Console_t *console, const bool up);
void ConsoleAddToHistory(Console_t *console, const char *input);
void ConsoleKeyInput(Console_t *console, char key);
void ConsoleDraw(Console_t *console);

#endif
