#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../font/font.h"
#include "console.h"

#ifdef WIN32
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

extern Font_t Fnt;

// Standard/basic console commands
void Console_CmdClear(Console_t *console, const char *param)
{
	Console_Clear(console);
}

void Console_CmdEcho(Console_t *console, const char *param)
{
	Console_Out(console, param);
}

bool Console_ExecFile(Console_t *console, const char *filename)
{
	FILE *stream=NULL;
	char buf[CONSOLE_MAX_COLUMN];

	if(!filename)
	{
		Console_Out(console, "No file specified");
		return false;
	}

	stream=fopen(filename, "r");

	if(stream==NULL)
	{
		sprintf(buf, "Unable to open file: %s", filename);
		Console_Out(console, buf);

		return false;
	}

	while(fgets(buf, CONSOLE_MAX_COLUMN, stream))
	{
		strtok(buf, " \n\f");
		char *param=buf+strlen(buf)+1;

		if(param[0]==0)
			param=NULL;

		if(!Console_ExecCommand(console, buf, param))
			Console_Out(console, "Invalid command in file");
	}

	fclose(stream);

	return true;
}

void Console_ClearCommandHistory(Console_t *console)
{
	memset(console->commandHistory, '\0', sizeof(char)*CONSOLE_MAX_COMMAND_HISTORY*CONSOLE_MAX_COMMAND_NAME);
	console->numCommandHistory=0;

	console->newCommand=0;
	console->currentCommand=0;
}

void Console_Clear(Console_t *console)
{
	memset(console->buffer, '\0', sizeof(char)*CONSOLE_MAX_ROW*CONSOLE_MAX_COLUMN);

	console->newLine=0;
	console->viewBottom=0;
}

void Console_Init(Console_t *console, const uint32_t width, const uint32_t height)
{
	console->column=1;

	console->width=width;
	console->height=height;

	console->numCommand=0;

	Console_AddCommand(console, "echo", Console_CmdEcho);
	Console_AddCommand(console, "clear", Console_CmdClear);
//	Console_AddCommand(console, "exec", Console_ExecFile);

	Console_Clear(console);
	Console_ClearCommandHistory(console);
	Console_Advance(console);

	console->buffer[console->newLine].text[0]=']';
	console->buffer[console->newLine].text[1]='_';
	console->buffer[console->newLine].text[2]='\0';
}

void Console_Destroy(Console_t *console)
{
}

void Console_Advance(Console_t *console)
{
	console->newLine--;

	if(console->newLine<0)
		console->newLine+=CONSOLE_MAX_SCROLLBACK;

	console->buffer[console->newLine].text[0]='\0';
	console->viewBottom=console->newLine;
}

void Console_Scroll(Console_t *console, const bool up)
{
	if(up)
	{
		console->viewBottom++;

		if(console->viewBottom>=CONSOLE_MAX_SCROLLBACK+CONSOLE_MAX_SCROLLBACK)
			console->viewBottom-=CONSOLE_MAX_SCROLLBACK+CONSOLE_MAX_SCROLLBACK;

		if(console->viewBottom==console->newLine)
			console->viewBottom--;
	}
	else
	{
		console->viewBottom--;

		if(console->viewBottom<0)
			console->viewBottom+=CONSOLE_MAX_SCROLLBACK;

		if(console->viewBottom==console->newLine-1)
			console->viewBottom=console->newLine;
	}
}

void Console_Out(Console_t *console, const char *string)
{
	Console_Advance(console);

	if(string)
		strcpy(console->buffer[console->newLine].text, string);
}

bool Console_AddCommand(Console_t *console, const char *commandName, void (*commandFunction)(Console_t *, const char *))
{
	if(console==NULL)
		return false;

	if(console->numCommand>=CONSOLE_MAX_COMMANDS)
		return false;

	strcpy(console->commands[console->numCommand].commandName, commandName);
	console->commands[console->numCommand].commandFunction=commandFunction;

	console->numCommand++;

	return true;
}

bool Console_ExecCommand(Console_t *console, const char *command, const char *param)
{
	if(console==NULL)
		return false;

	if(command==NULL)
		return false;

	// Search through command list and run it
	for(uint32_t i=0;i<console->numCommand;i++)
	{
		if(strcasecmp(command, console->commands[i].commandName)==0)
		{
			console->commands[i].commandFunction(console, param);
			return true;
		}
	}

	return false;
}

void Console_History(Console_t *console, const bool up)
{
	if(up)
	{
		console->currentCommand++;

		if(console->currentCommand>console->numCommandHistory)
			console->currentCommand=console->numCommandHistory;
	}
	else
	{
		console->currentCommand--;

		if(console->currentCommand<0)
			console->currentCommand=0;
	}

	if(console->currentCommand!=-1)
	{
		char buf[CONSOLE_MAX_COLUMN];

		buf[0]=']';
		buf[1]='\0';

		int32_t commandNumber=console->newCommand-console->currentCommand;

		if(commandNumber<0)
			commandNumber+=console->numCommandHistory;

		strcat(buf, console->commandHistory[commandNumber]);
		strcat(buf, "_");
		strcpy(console->buffer[console->newLine].text, buf);

		console->column=(int)strlen(buf)-1;
	}
}

void Console_Backspace(Console_t *console)
{
	// Backspace only up to the prompt character
	if(console->column>1)
	{
		console->buffer[console->newLine].text[console->column]='\0';
		console->buffer[console->newLine].text[console->column-1]='_';
		console->column--;
	}
}

void Console_Process(Console_t *console)
{
	// Terminate the line and process it
	console->buffer[console->newLine].text[console->column]='\0';

	char command[CONSOLE_MAX_COLUMN];

	// Copy the line out of the console buffer into a command buffer (not including the prompt character)
	strcpy(command, console->buffer[console->newLine].text+1);

	// Also place the command into the command history
	strcpy(console->commandHistory[console->newCommand], command);

	console->newCommand++;
	console->newCommand%=CONSOLE_MAX_COMMAND_HISTORY;
	console->currentCommand=0;

	console->numCommandHistory++;

	if(console->numCommandHistory>CONSOLE_MAX_COMMAND_HISTORY)
		console->numCommandHistory=CONSOLE_MAX_COMMAND_HISTORY;

	// Find the parameter after the command
	char *commandParam=strrchr(command, ' ');

	// If there was a parameter to the command
	if(commandParam!=NULL)
	{
		// Replace the space with a null-terminator and advance to split the string
		//     around the space and separate command and parameter.
		*commandParam='\0';
		commandParam++;
	}

	if(!Console_ExecCommand(console, command, commandParam))
		Console_Out(console, "Invalid command");

	// New line, prompt and cursor
	Console_Advance(console);

	console->column=1;
	console->buffer[console->newLine].text[0]=']';
	console->buffer[console->newLine].text[1]='_';
	console->buffer[console->newLine].text[2]='\0';
}

void Console_KeyInput(Console_t *console, const uint32_t keyCode)
{
	// Don't type past edge of console
	if(console->column>(signed)console->width-2)
		return;

	console->buffer[console->newLine].text[console->column++]=keyCode;
	console->buffer[console->newLine].text[console->column]='_';
	console->buffer[console->newLine].text[console->column+1]='\0';
}

void Console_Draw(Console_t *console)
{
	if(!console->active)
		return;

	uint32_t temp=console->viewBottom;

	for(uint32_t i=0;i<console->height;i++)
	{
		Font_Print(&Fnt, 16.0f, 0.0f, (float)i*16.0f, "%s", console->buffer[temp].text);
		temp=(temp+1)%CONSOLE_MAX_ROW;
	}
}
