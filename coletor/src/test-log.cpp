
struct pg_log_meta_data
{
	std::string first_line;
	std::string last_line;
	size_t last_line_pos;
	time_t first_time;
	time_t last_time;
	bool empty;
};

pg_log_meta_data get_log_meta_data(const std::string &filename)
{
	std::ifstream ifs;
	size_t length;
	pg_log_meta_data ret;
	size_t offset;
	size_t pos;
	const size_t BUFF_SIZE = 8192;
	char buffer[BUFF_SIZE];
	char c;
	ifs.exceptions(std::ios::failbit | std::ios::badbit);
	ifs.open(filename.c_str(), std::ios::in | std::ios::binary);
	ifs.exceptions(std::ios::failbit);
	if (!std::getline(ifs, ret.first_line))
	{
		ret.empty = true;
		return ret;
	}
	ret.empty = false;
	ifs.seekg(0, std::ios::end);
	length = ifs.tellg();
	while(ifs)
	{
		ifs.unget();
		ifs.unget();
		c = ifs.get();
		if (c == '\n')
		{
			size_t cur_pos = ifs.tellg();
			std::getline(ifs, ret.last_line);
			if (ret.last_line.empty() || ret.last_line[0] == '\t')
			{
				/* Ignore blank lines or those that not start a log */
				ifs.seekg(cur_pos, std::ios::beg);
			}
			else
			{
				ret.last_line_pos = ifs.tellg();
				break;
			}
		}
	}
	if (!ifs)
	{
		ret.last_line = ret.first_line;
	}
	DMSG(
		"first_line: " << ret.first_line
		<< "\n           last_line : " << ret.last_line
		<< "\n           last_pos  : " << ret.last_line_pos
	);
	std::istringstream iss(ret.first_line);
	std::tm ot;
	std::string date, time, tz;
	time_t oet;
	iss
			>> date
			>> time
			>> tz;
	::strptime((date + " " + time).c_str(), "%F %T", &ot);
	oet = Util::time::mktime(ot, tz);
	DMSG("Start epoch: " << oet);
	return ret;
}

