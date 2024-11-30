#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../font/font.h"
#include "console.h"

void ConsoleInit(Console_t *console)
{
    memset(console, 0, sizeof(Console_t));

    ConsoleRegisterCommand(console, "clear", ConsoleClear);
    ConsoleRegisterCommand(console, "echo", ConsolePrint);
    ConsoleRegisterCommand(console, "listcommands", ConsolePrintCommands);
    ConsoleRegisterCommand(console, "listvariables", ConsolePrintVariables);

    ConsoleRegisterVariable(console, "testvar", 123.0f);
}

void ConsolePrint(Console_t *console, const char *text)
{
    if(console->numLine>=CONSOLE_MAX_LINES)
    {
        // Scroll up to make room for the new line
        memmove(&console->lines[0], &console->lines[1], (CONSOLE_MAX_LINES-1)*sizeof(ConsoleLine_t));
        strncpy(console->lines[CONSOLE_MAX_LINES-1].buffer, text, CONSOLE_LINE_LENGTH-1);
    }
    else
    {
        // Add text to the next available line (which should be at the bottom)
        strncpy(console->lines[console->numLine].buffer, text, CONSOLE_LINE_LENGTH-1);
        console->numLine++;
    }
}

void ConsolePrintCommands(Console_t *console, const char *param)
{
    (void)param; // Unused

    ConsolePrint(console, "List of console commands:");
    for(uint32_t i=0;i<console->numCommand;i++)
        ConsolePrint(console, console->commands[i].name);
}

void ConsolePrintVariables(Console_t *console, const char *param)
{
    (void)param; // Unused

    ConsolePrint(console, "List of console variables:");
    for(uint32_t i=0;i<console->numVariable;i++)
        ConsolePrint(console, console->variables[i].name);
}

void ConsoleClear(Console_t *console, const char *param)
{
    (void)param; // Unused

    memset(console->lines, 0, sizeof(console->lines));
    console->numLine=0;
}

void ConsoleRegisterCommand(Console_t *console, const char *name, ConsoleCommandFunc func)
{
    if(console->numCommand>=CONSOLE_MAX_COMMANDS)
    {
        printf("Command list full!\n");
        return;
    }

    strncpy(console->commands[console->numCommand].name, name, CONSOLE_MAX_NAME-1);
    console->commands[console->numCommand].func=func;
    console->numCommand++;
}

void ConsoleRegisterVariable(Console_t *console, const char *name, float value)
{
    if(console->numVariable>=CONSOLE_MAX_VARIABLES)
    {
        printf("Variable list full!\n");
        return;
    }

    strncpy(console->variables[console->numVariable].name, name, CONSOLE_MAX_NAME-1);
    console->variables[console->numVariable].value=value;
    console->numVariable++;
}

void ConsoleSetVariable(Console_t *console, const char *name, float value)
{
    for(uint32_t i=0; i<console->numVariable; i++)
    {
        if(strncmp(console->variables[i].name, name, CONSOLE_MAX_NAME)==0)
        {
            console->variables[i].value=value;
            return;
        }
    }

    ConsolePrint(console, "Variable not found!");
}

float ConsoleGetVariable(Console_t *console, const char *name)
{
    for(uint32_t i=0; i<console->numVariable; i++)
    {
        if(strncmp(console->variables[i].name, name, CONSOLE_MAX_NAME)==0)
            return console->variables[i].value;
    }

    ConsolePrint(console, "Variable not found!");
    return 0.0f;
}

void ConsoleScroll(Console_t *console, const bool up)
{
    if(up)
    {
        if(console->numLine>0)
        {
            if(console->scrollbackOffset<console->numLine-1)
                console->scrollbackOffset++;
        }
    }
    else
    {
        if(console->scrollbackOffset>0)
            console->scrollbackOffset--;
        else
            console->scrollbackOffset=0;
    }
}

void ConsoleExecuteCommand(Console_t *console, const char *input)
{
    char name[CONSOLE_MAX_NAME]={ 0 };
    const char *args=NULL;

    // Split the input between the name and arguments
    const char *separatorToken=strchr(input, ' ');

    if(separatorToken)
    {
        strncpy(name, input, separatorToken-input);
        args=separatorToken+1;
    }
    else
        strncpy(name, input, CONSOLE_MAX_NAME-1);

    // Search variable list first
    for(uint32_t i=0; i<console->numVariable; i++)
    {
        if(strncmp(name, console->variables[i].name, CONSOLE_MAX_NAME)==0)
        {
            // If there's a value, set the variable, if no value, just print the variable's value
            if(args)
            {
                float value=atof(args);
                ConsoleSetVariable(console, name, value);
            }
            else
            {
                char value[CONSOLE_MAX_NAME]={ 0 };
                snprintf(value, CONSOLE_MAX_NAME, "%f", console->variables[i].value);
                ConsolePrint(console, value);
            }

            return;
        }
    }

    // Then search command list
    for(uint32_t i=0;i<console->numCommand;i++)
    {
        if(strncmp(name, console->commands[i].name, CONSOLE_MAX_NAME)==0)
        {
            if(console->commands[i].func)
            {
                console->commands[i].func(console, args?args:"");
                return;
            }
        }
    }

    ConsolePrint(console, "Unknown command/variable.");
}

void ConsoleHistory(Console_t *console, const bool up)
{
    if(up)
    {
        if(console->historyIndex>0)
        {
            console->historyIndex--;
            strncpy(console->input, console->history[console->historyIndex], CONSOLE_LINE_LENGTH-1);
            console->inputLength=strlen(console->input);
        }
    }
    else
    {
        if(console->historyIndex<console->numHistory-1)
        {
            console->historyIndex++;
            strncpy(console->input, console->history[console->historyIndex], CONSOLE_LINE_LENGTH-1);
            console->inputLength=strlen(console->input);
        }
        else if(console->historyIndex==console->numHistory-1)
        {
            console->historyIndex=console->numHistory;
            memset(console->input, 0, sizeof(console->input));
            console->inputLength=0;
        }
    }
}

void ConsoleAddToHistory(Console_t *console, const char *input)
{
    if(console->numHistory>=CONSOLE_MAX_HISTORY)
    {
        // Scroll up the history
        memmove(&console->history[0], &console->history[1], (CONSOLE_MAX_HISTORY-1)*CONSOLE_LINE_LENGTH);
        strncpy(console->history[CONSOLE_MAX_HISTORY-1], input, CONSOLE_LINE_LENGTH-1);
    }
    else
    {
        strncpy(console->history[console->numHistory], input, CONSOLE_LINE_LENGTH-1);
        console->numHistory++;
    }

    console->historyIndex=console->numHistory;
}

void ConsoleAutocomplete(Console_t *console)
{
    char matches[CONSOLE_MAX_COMMANDS+CONSOLE_MAX_VARIABLES][CONSOLE_MAX_NAME];
    uint32_t numMatches=0;

    // Find command matches
    for(uint32_t i=0;i<console->numCommand;i++)
    {
        if(strncmp(console->input, console->commands[i].name, console->inputLength)==0)
            strncpy(matches[numMatches++], console->commands[i].name, CONSOLE_MAX_NAME-1);
    }

    // Find variable matches
    for(uint32_t i=0; i<console->numVariable; i++)
    {
        if(strncmp(console->input, console->variables[i].name, console->inputLength)==0)
            strncpy(matches[numMatches++], console->variables[i].name, CONSOLE_MAX_NAME-1);
    }

    if(numMatches==1)
    {
        // Single match: auto-complete
        strncpy(console->input, matches[0], CONSOLE_LINE_LENGTH-1);
        console->inputLength=strlen(console->input);
    }
    else
    {
        // Multiple matches: display options
        for(uint32_t i=0;i<numMatches;i++)
            ConsolePrint(console, matches[i]);
    }
}

void ConsoleKeyInput(Console_t *console, char key)
{
    if(key=='\n')
    {
        // Enter key
        if(console->inputLength>0)
        {
            ConsoleAddToHistory(console, console->input);
            ConsolePrint(console, console->input);
            ConsoleExecuteCommand(console, console->input);
            memset(console->input, 0, sizeof(console->input));
            console->inputLength=0;
        }
    }
    else if(key=='\b')
    {
        // Backspace
        if(console->inputLength>0)
            console->input[--console->inputLength]='\0';
    }
    else if(key=='\t')
    {
        // Tab (autocomplete placeholder)
        ConsoleAutocomplete(console);
    }
    else if(key>=32&&key<127)
    { 
        // Printable characters
        if(console->inputLength<CONSOLE_LINE_LENGTH-1)
        {
            console->input[console->inputLength++]=key;
            console->input[console->inputLength]='\0';
        }
    }
}

void ConsoleDraw(Console_t *console)
{
    extern Font_t font;

    if(!console->active)
        return;

    const int maxLines=5;
    uint32_t startLine=(console->numLine>maxLines+console->scrollbackOffset)?console->numLine-maxLines-console->scrollbackOffset:0;
    uint32_t endLine=console->numLine-console->scrollbackOffset;

    if(startLine<0)
        startLine=0;

    if(endLine>console->numLine)
        endLine=console->numLine;

    // Print window of maxLines, y position is such that the most recent entry is near the prompt
    for(uint32_t i=startLine;i<endLine;i++)
        Font_Print(&font, 16.0f, 0.0f, (float)(endLine-i-1)*16.0f+100.0f, "%s", console->lines[i].buffer);

    // Print prompt
    Font_Print(&font, 16.0f, 0.0f, (float)100.0f-16.0f, "> %s", console->input);
}
