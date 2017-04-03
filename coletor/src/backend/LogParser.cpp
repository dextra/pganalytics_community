#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <sstream>

#include "debug.h"
#include "config.h"

#include "backend/LineHandler.h"
#include "backend/LogParser.h"

#define ER_FOR_PREFIX_ID "([0-9a-zA-Z\\_\\[\\]\\-]*)"
/* application name */
#define ER_FOR_PREFIX_APPNAME "([0-9a-zA-Z\\.\\-\\_\\/\\[\\]]*)"
/* user name */
#define ER_FOR_PREFIX_USERNAME ER_FOR_PREFIX_APPNAME
/* database name */
#define ER_FOR_PREFIX_DBNAME ER_FOR_PREFIX_APPNAME
/* remote host and port */
#define ER_FOR_PREFIX_REMOTE_HOST_PORT "([a-zA-Z0-9\\-\\.]+|\\[local\\]|\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})?[\\(\\d\\)]*"
/* remote host */
#define ER_FOR_PREFIX_REMOTE_HOST "([a-zA-Z0-9\\-\\.]+|\\[local\\]|\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})?"
/* process ID */
#define ER_FOR_PREFIX_PID "(\\d+)"
/* timestamp without milliseconds */
#define ER_FOR_PREFIX_TIMESTAMP "(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}(?: [A-Z\\d]{3,6})?)"
/* timestamp with milliseconds */
#define ER_FOR_PREFIX_TIMESTAMP_MS "(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d+(?: [A-Z\\d]{3,6})?)"
/* command tag */
#define ER_FOR_PREFIX_COMMAND_TAG "([0-9a-zA-Z\\.\\-\\_]*)"
/* SQL state */
#define ER_FOR_PREFIX_SQL_STATE "([0-9a-zA-Z]+)"
/* session ID */
#define ER_FOR_PREFIX_SESSION_ID "([0-9a-f\\.]*)"
/* session line number */
#define ER_FOR_PREFIX_SESSION_LINE "(\\d+)"
/* session start timestamp */
#define ER_FOR_PREFIX_SESSION_TIMESTAMP ER_FOR_PREFIX_TIMESTAMP
/* virtual transaction ID */
#define ER_FOR_PREFIX_VXID "([0-9a-f\\.\\/]*)"
/* transaction ID (0 if none) */
#define ER_FOR_PREFIX_XID "([0-9a-f\\.\\/]*)"
/* stop here in non-session */
#define ER_FOR_PREFIX_NON_SESSION_STOP ""
/* '%' */
#define ER_FOR_PREFIX_PERCENT "%"
#define ER_FOR_LOG_TYPE "(LOG|WARNING|ERROR|FATAL|PANIC|DETAIL|HINT|STATEMENT|CONTEXT|QUERY|LOCATION): "

//#define ER_FOR_IDENTIFIER "([0-9a-zA-Z\\_\\[\\]\\-]*)"
#define ER_FOR_IDENTIFIER "(\"(?:[^\"]|\"\")*\"|[a-z_][a-z_0-9]*)"
#define ER_FOR_INTERVAL "(\\d+:\\d{2}:\\d{2}\\.\\d+)"
#define ER_FOR_INTEGER "(\\d+)"
#define ER_FOR_FLOAT "(\\d+\\.\\d+)"
#define ER_FOR_CONNECTION_AUTHORIZED \
	"connection authorized: user=" ER_FOR_PREFIX_APPNAME " database=" ER_FOR_PREFIX_APPNAME ""
#define ER_FOR_DISCONNECTION \
	"disconnection: session time: " ER_FOR_INTERVAL " user=" ER_FOR_PREFIX_APPNAME " database=" ER_FOR_PREFIX_APPNAME " host=" ER_FOR_PREFIX_REMOTE_HOST "(?:  port=\\d+)?"
#define ER_FOR_MIN_DURATION_STATEMENT \
	"duration: (\\d+\\.\\d+) ms  statement: (.*)"
#define ER_FOR_STATEMENT \
	"statement: (.*)"
#define ER_FOR_DURATION \
	"duration: (.*)"
#define ER_FOR_AUTOVACUUM \
	"^automatic vacuum of table " ER_FOR_IDENTIFIER ": index scans: " ER_FOR_INTEGER "\\s*"             \
	"pages: " ER_FOR_INTEGER " removed, " ER_FOR_INTEGER " remain\\s*"                                  \
	"tuples: " ER_FOR_INTEGER " removed, " ER_FOR_INTEGER " remain\\s*"                                 \
	"buffer usage: " ER_FOR_INTEGER " hits, " ER_FOR_INTEGER " misses, " ER_FOR_INTEGER " dirtied\\s*"  \
	"avg read rate: " ER_FOR_FLOAT " MB/s, avg write rate: " ER_FOR_FLOAT " MB/s\\s*"                   \
	"system usage: CPU " ER_FOR_FLOAT "s/" ER_FOR_FLOAT "u sec elapsed " ER_FOR_FLOAT " sec\\s*"        \
	"$"
#define ER_FOR_AUTOANALYZE \
	"automatic analyze of table " ER_FOR_IDENTIFIER " system usage: CPU " ER_FOR_FLOAT "s/" ER_FOR_FLOAT "u sec elapsed " ER_FOR_FLOAT " sec"
#define ER_FOR_CHECKPOINT_START \
	"checkpoint starting: ([a-z]*)"
#define ER_FOR_CHECKPOINT_END \
	"checkpoint complete: wrote " ER_FOR_INTEGER " buffers \\(" ER_FOR_FLOAT "%\\); " ER_FOR_INTEGER " transaction log file\\(s\\) added, " ER_FOR_INTEGER " removed, " ER_FOR_INTEGER " recycled; write=" ER_FOR_FLOAT " s, sync=" ER_FOR_FLOAT " s, total=" ER_FOR_FLOAT " s; sync files=" ER_FOR_INTEGER ", longest=" ER_FOR_FLOAT " s, average=" ER_FOR_FLOAT " s"

#define ER_FOR_TEMPORARY_FILE \
	"temporary file: path \"([^\"]*)\", size " ER_FOR_INTEGER

#define PARAM_REPLACEMENT '?'
#define IN_LIST_REPLACEMENT "..."

char to_upper_if_ascii(char c)
{
	if (c >= 'a' && c <= 'z')
	{
		return (c - ('a' - 'A'));
	}
	return c;
}

static bool is_space(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static bool is_starting_number(const std::string &val, size_t i)
{
	/* If the previous character is [a-z_] means it may be an identifier, or [0-9.] it is a number skipped already */
	if (i)
	{
		char prev_char = to_upper_if_ascii(val[i-1]);
		if ((prev_char >= 'A' && prev_char <= 'Z') || (prev_char == '_') || (prev_char >= '0' && prev_char <= '9') || (prev_char == '.'))
		{
			return false;
		}
	}
	if (val[i] == '+' || val[i] == '-')
	{
		i++;
	}
	return ((val[i] >= '0' && val[i] <= '9') || (val[i] == '.' && val[i+1] >= '0' && val[i+1] <= '9'));
}

/**
 * OBS: This function increments "i" if needed to match the quote
 */
bool is_starting_string(const std::string &val, size_t &i)
{
	if (i)
	{
		char prev_char = to_upper_if_ascii(val[i-1]);
		char curr_char = to_upper_if_ascii(val[i]);
		/**
		 * Check if E'...', N'...', B'...' or X'...' formats: last character
		 * must not be between [A-Z], the current must be [ENBX] and the next
		 * must be a single quote.
		 */
		if (!(prev_char >= 'A' && prev_char <= 'Z') && (curr_char == 'E' || curr_char == 'N' || curr_char == 'B' || curr_char == 'X') && (val[i+1] == '\''))
		{
			/* Increment to be in the quote */
			i++;
			return true;
		}
	}
	return val[i] == '\'';
}

bool is_starting_keyword(const std::string &val, size_t i, const std::string &keyword /* keyword must be UPPERCASE! */)
{
	if (i)
	{
		char prev_char = to_upper_if_ascii(val[i-1]);
		if (prev_char >= 'A' && prev_char <= 'Z')
		{
			return false;
		}
	}
	size_t j = 0;
	char c;
	for (j = 0; j < keyword.size() && i < val.size(); j++, i++)
	{
		if (keyword[j] != to_upper_if_ascii(val[i]))
		{
			return false;
		}
	}
	c = to_upper_if_ascii(val[i]);
	return (j == keyword.size() && (c <= 'A' || c >= 'Z'));
}

bool is_starting_in_list(const std::string &val, size_t i, size_t &ppos)
{
	if (is_starting_keyword(val, i, "IN"))
	{
		/* Look for open parenthesis */
		i += 2;
		while (is_space(val[i]))
		{
			i++;
		}
		if (val[i] == '(')
		{
			ppos = i;
			return true;
		}
	}
	return false;
}

static std::string normalize_statement(const std::string &val)
{
	std::string ret;
	bool inside_in_list = false;
	size_t i_aux;
	for (size_t i = 0; i < val.size(); i++)
	{
		/**
		 * Starting a STRING
		 * (note that "i" is passed byref and can be incremented for
		 * [ENBX]'...' formats)
		 */
		if (is_starting_string(val, i))
		{
			if (!inside_in_list)
			{
				ret += PARAM_REPLACEMENT;
			}
			for (i++; i < val.size(); i++)
			{
				if (val[i] == '\'')
				{
					if (val[i+1] == '\'')
					{
						/* the next character is escaping the quote as '' */
						i++; /* skip this and the next one */
					}
					else if (val[i-1] == '\\')
					{
						/* the previous character was escaping this quote as \' */
						/* do nothing, just skip this */
					}
					else
					{
						/* end of string */
						break;
					}
				}
			}
		}
		/**
		 * Starting a QUOTED IDENTIFIER
		 */
		else if (val[i] == '"')
		{
			if (!inside_in_list)
			{
				ret += val[i];
			}
			for (i++; i < val.size(); i++)
			{
				if (!inside_in_list)
				{
					ret += val[i];
				}
				if (val[i] == '"')
				{
					if (val[i+1] == '"')
					{
						i++; /* skip next */
						if (!inside_in_list)
						{
							ret += val[i];
						}
					}
					else
					{
						break;
					}
				}
			}
		}
		/**
		 * Starting a NUMBER (integer, signed, unsigned, decimal, etc.)
		 */
		else if (is_starting_number(val, i))
		{
			bool process_e_notation;
			bool processed_one_e_notation = false;
			if (!inside_in_list)
			{
				ret += PARAM_REPLACEMENT;
			}
			do
			{
				bool found_dot = false;
				processed_one_e_notation = process_e_notation;
				process_e_notation = false;
				if (val[i] == '-' || val[i] == '+')
				{
					i++;
				}
				for (; i < val.size(); i++)
				{
					if (val[i] >= '0' && val[i] <= '9')
					{
						/* skip */
					}
					else if (!found_dot && val[i] == '.')
					{
						found_dot = true;
					}
					else if (!processed_one_e_notation && (val[i] == 'e' || val[i] == 'E'))
					{
						process_e_notation = true;
						i++;
						break;
					}
					else
					{
						i--;
						break;
					}
				}
			}
			while(!processed_one_e_notation && process_e_notation);
		}
		/**
		 * Replace WHITE SPACES to a single space (also tabs, break lines, etc.)
		 */
		else if (!inside_in_list && is_space(val[i]))
		{
			while (i+1 < val.size() && is_space(val[i+1]))
			{
				i++;
			}
			ret += ' ';
		}
		/**
		 * Starting an IN LIST
		 */
		else if (!inside_in_list && is_starting_in_list(val, i, i_aux))
		{
			inside_in_list = true;
			/* Append IN keyword */
			ret += val[i];
			ret += val[++i];
			/* Append open parenthesis */
			ret += " (";
			ret += IN_LIST_REPLACEMENT;
			/* Increment *at* open parenthesis */
			i = i_aux;
		}
		else
		{
			/**
			 * Close IN LIST
			 */
			if (inside_in_list && val[i] == ')')
			{
				ret += val[i];
				inside_in_list = false;
			}
			/**
			 * Any other character is just appended
			 */
			else if (!inside_in_list)
			{
				ret += val[i];
			}
		}
	}
	return ret;
}

static void copy_str_as_value(const std::string &value, std::ostream &str)
{
	for (size_t i = 0; i < value.length(); i++)
	{
		/* Escapes accepted for COPY (from table at http://www.postgresql.org/docs/9.3/static/sql-copy.html#AEN69021) */
		switch (value[i])
		{
		case '\\':
			str << "\\\\";
			break;
		case '\b':
			str << "\\b";
			break;
		case '\f':
			str << "\\f";
			break;
		case '\n':
			str << "\\n";
			break;
		case '\r':
			str << "\\r";
			break;
		case '\t':
			str << "\\t";
			break;
		case '\v':
			str << "\\v";
			break;
		default:
			str << value[i];
		}
	}
}

static void IGNORE_UNUSED_WARN normalize_statement_old(std::string &stmt)
{
	/*
	"([[:<:]](\\d+\\.\\d+|\\d+)[[:>:]] | => numbers (integers and decimals)
	'([^']|'')*'|E'([^']|''|\\\\')*')'" => strings, either normal ones ('...') or escaped (E'...')

	TODO: Consider standard_conforming_strings = off?
	*/
	static Poco::RegularExpression er_normalize_nonstandard_strings(
		"\\'",
		Poco::RegularExpression::RE_MULTILINE | Poco::RegularExpression::RE_NO_UTF8_CHECK
	);
	static Poco::RegularExpression er_normalize_params(
		//"([-+]?(?:\\d*\\.\\d+|\\d+)(?:[Ee][-+]?\\d+)?\\b|'(?:[^']|'')*'|E'(?:[^']|''|\\\\')*')"
		/* XXX: A number that starts with a dot (like .5) or a sign (-/+) we can't use \\b, so it is treated separately */
		/* That can be:
		 * - A number starting with dot or sign [+-], and ends an word
		 * - A number in the form D[.D][eD], that starts and end an word
		 * - A string in the form '...' or E'...'
		 */
		"((?:(?:[-+]|\\b)(?:\\d*\\.\\d+|\\d+))(?:[Ee][-+]?\\d+)?\\b|'(?:[^']|'')*'|E'(?:[^']|''|\\\\')*')",
		Poco::RegularExpression::RE_MULTILINE | Poco::RegularExpression::RE_NO_UTF8_CHECK
	);
	static Poco::RegularExpression er_normalize_in( /* XXX: Must be executed after er_normalize_params, so it can safely remove the expressions */
		"(IN)\\s*\\([^\\(]*\\)",
		Poco::RegularExpression::RE_CASELESS
	);
	static Poco::RegularExpression er_normalize_spaces(
		"\\s+",
		Poco::RegularExpression::RE_MULTILINE | Poco::RegularExpression::RE_DOTALL /* /ms */
	);
	//DMSG("er_normalize_spaces");
	//er_normalize_spaces.subst(stmt, " ", Poco::RegularExpression::RE_GLOBAL);
	DMSG("er_normalize_nonstandard_strings");
	er_normalize_nonstandard_strings.subst(stmt, "", Poco::RegularExpression::RE_GLOBAL);
	DMSG("er_normalize_params");
	er_normalize_params.subst(stmt, "?", Poco::RegularExpression::RE_GLOBAL);
	DMSG("er_normalize_in");
	er_normalize_in.subst(stmt, "$1 (...)", Poco::RegularExpression::RE_GLOBAL);
}


LogParser::LogParser(LineHandler &handler)
	: handler(handler)
{
	size_t info_size = ((size_t)E_PREFIX_MESSAGE)+1;
	this->lineParserRE = NULL;
	this->first_line = true;
	/* Allocate vectors */
	this->current_line.resize(info_size, std::string(""));
	this->current_info_available.resize(info_size, false);
	this->match_index.resize(info_size, 0);
	/* Reserve default spaces */
	this->current_line[(size_t)E_PREFIX_TIMESTAMP].reserve(25);
	this->current_line[(size_t)E_PREFIX_TYPE].reserve(12);
	this->current_line[(size_t)E_PREFIX_MESSAGE].reserve(8192);
}
LogParser::~LogParser()
{
	if (lineParserRE != NULL)
	{
		delete lineParserRE;
	}
}
void LogParser::generateLineParserRE(const std::string &prefix)
{
	size_t i = 0;
	size_t match_index_count = 1;
	std::string line_er = "^";
	bool non_session_stop = false;
	for (i = 0; i < prefix.length(); i++)
	{
		if (prefix[i] != '%')
		{
			switch(prefix[i])
			{
			case '.':
			case '^':
			case '[':
			case ']':
			case ')':
			case '(':
			case '{':
			case '}':
			case '|':
			case '\\':
				line_er.append("\\");
			default:
				line_er += prefix[i];
			}
		}
		else
		{
			i++;
			switch(prefix[i])
			{
				/* application name */
			case 'a':
				this->match_index[E_PREFIX_APPNAME] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_APPNAME);
				break;
				/* user name */
			case 'u':
				this->match_index[E_PREFIX_USERNAME] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_USERNAME);
				break;
				/* database name */
			case 'd':
				this->match_index[E_PREFIX_DBNAME] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_DBNAME);
				break;
				/* remote host and port */
			case 'r':
				this->match_index[E_PREFIX_REMOTE_HOST_PORT] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_REMOTE_HOST_PORT);
				break;
				/* remote host */
			case 'h':
				this->match_index[E_PREFIX_REMOTE_HOST] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_REMOTE_HOST);
				break;
				/* process ID */
			case 'p':
				this->match_index[E_PREFIX_PID] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_PID);
				break;
				/* timestamp without milliseconds */
			case 't':
				this->match_index[E_PREFIX_TIMESTAMP] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_TIMESTAMP);
				break;
				/* timestamp with milliseconds */
			case 'm':
				this->match_index[E_PREFIX_TIMESTAMP_MS] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_TIMESTAMP_MS);
				break;
				/* command tag */
			case 'i':
				this->match_index[E_PREFIX_COMMAND_TAG] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_COMMAND_TAG);
				break;
				/* SQL state */
			case 'e':
				this->match_index[E_PREFIX_SQL_STATE] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_SQL_STATE);
				break;
				/* session ID */
			case 'c':
				this->match_index[E_PREFIX_SESSION_ID] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_SESSION_ID);
				break;
				/* session line number */
			case 'l':
				this->match_index[E_PREFIX_SESSION_LINE] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_SESSION_LINE);
				break;
				/* session start timestamp */
			case 's':
				this->match_index[E_PREFIX_SESSION_TIMESTAMP] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_SESSION_TIMESTAMP);
				break;
				/* virtual transaction ID */
			case 'v':
				this->match_index[E_PREFIX_VXID] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_VXID);
				break;
				/* transaction ID (0 if none) */
			case 'x':
				this->match_index[E_PREFIX_XID] = match_index_count++;
				line_er.append(ER_FOR_PREFIX_XID);
				break;
				/* stop here in non-session */
			case 'q':
				non_session_stop = true;
				line_er.append("(?:");
				break;
				/* escape % */
			case '%':
				line_er.append(ER_FOR_PREFIX_PERCENT);
				break;
			}
		}
	}
	if (non_session_stop)
	{
		line_er.append(")");
	}
	line_er.append(ER_FOR_LOG_TYPE);
	this->match_index[E_PREFIX_TYPE] = match_index_count++;
	line_er.append("[\\s]*(.*)$");
	this->match_index[E_PREFIX_MESSAGE] = match_index_count++;
	DMSG("line_er: " << line_er << std::endl);
	this->lineParserRE = new Poco::RegularExpression(line_er);
}

void LogParser::sendLogBuffer()
{
	if (!this->first_line)
	{
		std::ostringstream str;
		/*
		//Poco::RegularExpression er_autovacuum(ER_FOR_AUTOVACUUM, Poco::RegularExpression::RE_MULTILINE);
		Poco::RegularExpression er_checkpointstart(ER_FOR_CHECKPOINT_START);
		Poco::RegularExpression er_autovacuum(ER_FOR_AUTOVACUUM, Poco::RegularExpression::RE_MULTILINE | Poco::RegularExpression::RE_DOTALL);
		Poco::RegularExpression::MatchVec matches;
		if (er_checkpointstart.match(this->currentLogInfoFor(E_PREFIX_MESSAGE))) {
			DMSG("C H E C K P O I N T:\n");
		}
		if (er_autovacuum.match(this->currentLogInfoFor(E_PREFIX_MESSAGE))) {
			DMSG("A U T O V A C U U M:\n");
		}
		*/
		for (size_t i = 0; i <= (size_t)E_PREFIX_MESSAGE; i++)
		{
			if (i) str << "\t";
			if (this->isCurrentLogInfoAvailable((LinePrefixEnum)i))
			{
				const std::string &item = this->currentLogInfoFor((LinePrefixEnum)i);
				copy_str_as_value(item, str);
			}
			else
			{
				str << "\\N";
			}
		}
		str << "\t";
		copy_str_as_value(normalize_statement(this->current_line[this->match_index[(size_t)E_PREFIX_MESSAGE]]), str);
		str.flush();
		this->handler.handleLine(str.str());
		this->first_line = true;
		this->current_info_available.assign(this->current_info_available.size(), false);
	}
}

void LogParser::consume(std::string &line)
{
	Poco::RegularExpression::MatchVec matches;
	if (!this->first_line && line[0] == '\t')
	{
		line[0] = '\n';
		this->appendMessage(line);
	}
	else if (this->lineParserRE->match(line, 0, matches))
	{
		this->sendLogBuffer();
		for (size_t i = 0; i <= E_PREFIX_MESSAGE; i++)
		{
			if (this->isLogInfoAvailable((LinePrefixEnum)i))
			{
				/* If the string wasn't match, just ignore it and it'll become NULL */
				if (matches[this->logInfoIndex((LinePrefixEnum)i)].offset != std::string::npos)
				{
					this->currentLogInfoFor((LinePrefixEnum)i, line.substr(
												matches[this->logInfoIndex((LinePrefixEnum)i)].offset,
												matches[this->logInfoIndex((LinePrefixEnum)i)].length
											));
				}
			}
			this->first_line = false;
		}
	}
	else
	{
		this->sendLogBuffer();
		//DMSG("Line doesn't match: " << line << std::endl);
		DMSG("Line doesn't match: " << line);
	}
}
void LogParser::finalize()
{
	this->sendLogBuffer();
}

