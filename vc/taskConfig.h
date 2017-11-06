#pragma once

typedef struct Config {
	char * ip;
	int port;
	int threadCount;
	long active;
}Config;

int parseConfig(Config *config, int argc, char* argv[]);

void dumpConfig(Config *config);

int Task(Config * cfg);