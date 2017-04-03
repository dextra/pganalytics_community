#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include "LogParser.h"
#include "LineHandler.h"

int process(std::istream &in, std::ostream &out, const std::string &log_line_prefix)
{
	StreamLineHandler handler(out);
	LogParser parser(handler);
	std::string line;
	parser.generateLineParserRE(log_line_prefix);
	std::cout << "COPY postgres_log FROM stdin;" << std::endl;
	while(std::getline(std::cin, line))
	{
		parser.consume(line);
	}
	parser.finalize();
	std::cout << "\\." << std::endl;
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " log_line_prefix" << std::endl;
		std::cerr << std::endl;
		std::cerr
				<< "    This program is PoC that gets a PostgreSQL log file in its\n"
				<< "    stdout and prints a COPY statement to load it into postgres_log\n"
				<< "    table. See postgres_log.sql for query examples and postgres_log\n"
				<< "    definition"
				<< std::endl;
		return (argc == 1 ? 0 : 1);
	}
	return process(std::cin, std::cout, argv[1]);
}

