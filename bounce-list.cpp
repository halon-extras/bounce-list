#include <HalonMTA.h>
#include <list>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <fstream>
#include <sstream>
#include <string.h>
#include <syslog.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <csv.h>

extern char *__progname;

struct pcrePattern
{
	pcre2_code* pcre_;
	std::string value;
	std::string pattern;
};

class bouncepatterns
{
	public:
		std::string path;
		bool autoreload = true;
		std::map<std::string, std::map<std::string, std::list<pcrePattern>>> grouping_state_pattern;
		std::map<std::string, std::list<pcrePattern>> grouping_default_state_pattern;
		std::map<std::string, std::list<pcrePattern>> default_grouping_state_pattern;
		std::list<pcrePattern> default_grouping_default_state_pattern;
		std::list<pcre2_code*> regexs;
		~bouncepatterns()
		{
			for (auto re : regexs)
				pcre2_code_free(re);
		}
};

static void list_open(const std::string& list, const std::string& path, bool autoreload);
static std::pair<std::string, std::string> list_lookup(const std::string& list, const std::string& grouping, const std::string& state, const std::string& message);
static void list_reopen(const std::string& list);
static void list_parse(const std::string& path, std::shared_ptr<bouncepatterns> list);

static std::mutex listslock;
static std::map<std::string, std::shared_ptr<bouncepatterns>> lists;

HALON_EXPORT
int Halon_version()
{
	return HALONMTA_PLUGIN_VERSION;
}

HALON_EXPORT
bool Halon_init(HalonInitContext* hic)
{
	HalonConfig* cfg;
	HalonMTA_init_getinfo(hic, HALONMTA_INIT_CONFIG, nullptr, 0, &cfg, nullptr);

	try {
		auto lists_ = HalonMTA_config_object_get(cfg, "lists");
		if (lists_)
		{
			size_t l = 0;
			HalonConfig* list;
			while ((list = HalonMTA_config_array_get(lists_, l++)))
			{
				const char* id = HalonMTA_config_string_get(HalonMTA_config_object_get(list, "id"), nullptr);
				const char* path = HalonMTA_config_string_get(HalonMTA_config_object_get(list, "path"), nullptr);
				const char* autoreload = HalonMTA_config_string_get(HalonMTA_config_object_get(list, "autoreload"), nullptr);
				if (!id || !path)
					continue;
				list_open(id, path, !autoreload || strcmp(autoreload, "true") == 0);
			}
		}
		return true;
	} catch (const std::runtime_error& e) {
		syslog(LOG_CRIT, "%s", e.what());
		return false;
	}
}

HALON_EXPORT
void Halon_config_reload(HalonConfig* cfg)
{
	for (auto & list : lists)
	{
		listslock.lock();
		if (!list.second->autoreload)
		{
			listslock.unlock();
			continue;
		}
		listslock.unlock();

		try {
			list_reopen(list.first);
		} catch (const std::runtime_error& e) {
			syslog(LOG_CRIT, "%s", e.what());
		}
	}
}

HALON_EXPORT
bool Halon_command_execute(HalonCommandExecuteContext* hcec, size_t argc, const char* argv[], size_t argvl[], char** out, size_t* outlen)
{
	try {
		if (argc > 1 && strcmp(argv[0], "reload") == 0)
		{
			list_reopen(argv[1]);
			*out = strdup("OK");
			return true;
		}
		if (argc > 2 && strcmp(argv[0], "test") == 0)
		{
			auto t = list_lookup(argv[1], argv[2], argc > 3 ? argv[3] : "", argc > 4 ? argv[4] : "");
			*out = strdup((t.first + "\n" + t.second).c_str());
			return true;
		}
		throw std::runtime_error("No such command");
	} catch (const std::runtime_error& e) {
		*out = strdup(e.what());
		return false;
	}
}

static void bounce_list(HalonHSLContext* hhc, HalonHSLArguments* args, HalonHSLValue* ret)
{
	HalonHSLValue* x;
	char* id = nullptr;
	char* grouping = nullptr;
	char* state = nullptr;
	char* text = nullptr;
	size_t textlen = 0;

	x = HalonMTA_hsl_argument_get(args, 0);
	if (x && HalonMTA_hsl_value_type(x) == HALONMTA_HSL_TYPE_STRING)
		HalonMTA_hsl_value_get(x, HALONMTA_HSL_TYPE_STRING, &id, nullptr);
	else
	{
		HalonHSLValue* e = HalonMTA_hsl_throw(hhc);
		HalonMTA_hsl_value_set(e, HALONMTA_HSL_TYPE_EXCEPTION, "Bad id parameter", 0);
		return;
	}

	x = HalonMTA_hsl_argument_get(args, 1);
	if (x && HalonMTA_hsl_value_type(x) == HALONMTA_HSL_TYPE_STRING)
		HalonMTA_hsl_value_get(x, HALONMTA_HSL_TYPE_STRING, &text, &textlen);
	else
	{
		HalonHSLValue* e = HalonMTA_hsl_throw(hhc);
		HalonMTA_hsl_value_set(e, HALONMTA_HSL_TYPE_EXCEPTION, "Bad message parameter", 0);
		return;
	}

	x = HalonMTA_hsl_argument_get(args, 2);
	if (x)
	{
		if (HalonMTA_hsl_value_type(x) == HALONMTA_HSL_TYPE_STRING)
			HalonMTA_hsl_value_get(x, HALONMTA_HSL_TYPE_STRING, &grouping, nullptr);
		else if (HalonMTA_hsl_value_type(x) != HALONMTA_HSL_TYPE_NONE)
		{
			HalonHSLValue* e = HalonMTA_hsl_throw(hhc);
			HalonMTA_hsl_value_set(e, HALONMTA_HSL_TYPE_EXCEPTION, "Bad grouping parameter", 0);
			return;
		}
	}

	x = HalonMTA_hsl_argument_get(args, 3);
	if (x)
	{
		if (HalonMTA_hsl_value_type(x) == HALONMTA_HSL_TYPE_STRING)
			HalonMTA_hsl_value_get(x, HALONMTA_HSL_TYPE_STRING, &state, nullptr);
		else if (HalonMTA_hsl_value_type(x) != HALONMTA_HSL_TYPE_NONE)
		{
			HalonHSLValue* e = HalonMTA_hsl_throw(hhc);
			HalonMTA_hsl_value_set(e, HALONMTA_HSL_TYPE_EXCEPTION, "Bad state parameter", 0);
			return;
		}
	}

	try {
		auto r = list_lookup(id, std::string(text, textlen), grouping ? grouping : "", state ? state : "");
		if (r.first.empty())
			return;
		HalonHSLValue *k, *v;
		HalonMTA_hsl_value_array_add(ret, &k, &v);
		HalonMTA_hsl_value_set(k, HALONMTA_HSL_TYPE_STRING, "pattern", 0);
		HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_STRING, r.first.c_str(), r.first.size());
		HalonMTA_hsl_value_array_add(ret, &k, &v);
		HalonMTA_hsl_value_set(k, HALONMTA_HSL_TYPE_STRING, "value", 0);
		HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_STRING, r.second.c_str(), r.second.size());
	} catch (const std::runtime_error& ex) {
		HalonHSLValue* e = HalonMTA_hsl_throw(hhc);
		HalonMTA_hsl_value_set(e, HALONMTA_HSL_TYPE_EXCEPTION, ex.what(), 0);
		return;
	}
}

HALON_EXPORT
bool Halon_hsl_register(HalonHSLRegisterContext* ptr)
{
	HalonMTA_hsl_module_register_function(ptr, "bounce_list", &bounce_list);
	return true;
}

struct csv_parser_ptr
{
	std::vector<std::string> col;
	bouncepatterns* ptr;
	bool error = false;
};

static void cb1(void *s, size_t i, void *p)
{
	auto x = (csv_parser_ptr*)p;
	x->col.push_back(std::string((char*)s, i).c_str());
}

static void cb2(int c, void *p)
{
	auto x = (csv_parser_ptr*)p;
	if (x->col.size() < 1 || x->col.size() > 5)
	{
		x->error = true;
		return;
	}

	if (x->col[0].size() > 2 && x->col[0][0] == '/' && x->col[0][x->col[0].size() - 1] != '/')
	{
		x->error = true;
		return;
	}

	int errorcode;
	PCRE2_SIZE offset;
	pcre2_code* re = pcre2_compile((PCRE2_SPTR)(x->col[0].size() > 2 && x->col[0][0] == '/' && x->col[0][x->col[0].size() - 1] == '/' ? x->col[0].substr(1, x->col[0].size() - 2).c_str() : x->col[0].c_str()), PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errorcode, &offset, nullptr);
	if (!re)
	{
		x->error = true;
		return;
	}
	if (x->col.size() > 2 && !x->col[2].empty())
	{
		std::stringstream ss(x->col[2]);
		std::string group;
		while (std::getline(ss, group, ','))
		{
			if (x->col.size() > 3 && !x->col[3].empty())
				x->ptr->grouping_state_pattern[group][x->col[3]].push_back({ re, x->col[1], x->col[0] });
			else
				x->ptr->grouping_default_state_pattern[group].push_back({ re, x->col.size() > 1 ? x->col[1] : "", x->col[0] });
		}
	}
	else
	{
		if (x->col.size() > 3 && !x->col[3].empty())
			x->ptr->default_grouping_state_pattern[x->col[3]].push_back({ re, x->col[1], x->col[0] });
		else
			x->ptr->default_grouping_default_state_pattern.push_back({ re, x->col.size() > 1 ? x->col[1] : "", x->col[0] });
	}
	x->ptr->regexs.push_back(re);
}

static void list_parse(const std::string& path, std::shared_ptr<bouncepatterns> list)
{
	std::ifstream file(path);
	if (!file.good())
		throw std::runtime_error("Could not open file: " + path);

	csv_parser p;
	csv_parser_ptr ptr;
	ptr.ptr = list.get();
	csv_init(&p, 0);
	std::string line;
	size_t errors_format = 0, errors = 0, last_error = 0, i = 0;
	while (std::getline(file, line))
	{
		// skip comments
		if (line.size() > 0 && line[0] == '#')
			continue;
		++i;
		ptr.col.clear();
		ptr.error = false;
		if (csv_parse(&p, line.c_str(), line.size(), cb1, cb2, &ptr) != line.size())
		{
			++errors_format;
			last_error = i;
		}
		csv_fini(&p, cb1, cb2, &ptr);
		if (ptr.error)
		{
			++errors;
			last_error = i;
		}
	}
	csv_free(&p);
	file.close();
	if (strcmp(__progname, "smtpd") == 0)
		syslog(LOG_INFO, "bounce-list %s loaded, regexes: %zu, format-errors: %zu, data-errors %zu, last-error: %s",
			path.c_str(),
			list->regexs.size(),
			errors_format,
			errors,
			last_error > 0 ? std::to_string(last_error).c_str() : "n/a"
		);
}

static void list_open(const std::string& list, const std::string& path, bool autoreload)
{
	auto bouncelist = std::make_shared<bouncepatterns>();
	bouncelist->path = path;
	bouncelist->autoreload = autoreload;

	list_parse(bouncelist->path, bouncelist);

	listslock.lock();
	lists[list] = bouncelist;
	listslock.unlock();
}

static std::pair<std::string, std::string> list_lookup(const std::string& list, const std::string& message, const std::string& grouping, const std::string& state)
{
	listslock.lock();
	auto l = lists.find(list);
	if (l == lists.end())
	{
		listslock.unlock();
		throw std::runtime_error("No such list id");
	}
	auto bouncelist = l->second;
	listslock.unlock();

	pcre2_match_data* match_data = pcre2_match_data_create(1, nullptr);
	if (!grouping.empty() && !state.empty())
	{
		auto x = bouncelist->grouping_state_pattern.find(grouping);
		if (x != bouncelist->grouping_state_pattern.end())
		{
			auto x2 = x->second.find(state);
			if (x2 != x->second.end())
			{
				for (const auto & p : x2->second)
				{
					int rc = pcre2_match(p.pcre_,
								(PCRE2_SPTR)message.c_str(),
								message.size(),
								0,
								PCRE2_NO_UTF_CHECK,
								match_data,
								nullptr);
					if (rc >= 0)
					{
						pcre2_match_data_free(match_data);
						return { p.pattern, p.value };
					}
				}
			}
		}
	}
	if (!grouping.empty())
	{
		auto x = bouncelist->grouping_default_state_pattern.find(grouping);
		if (x != bouncelist->grouping_default_state_pattern.end())
		{
			for (const auto & p : x->second)
			{
				int rc = pcre2_match(p.pcre_,
							(PCRE2_SPTR)message.c_str(),
							message.size(),
							0,
							PCRE2_NO_UTF_CHECK,
							match_data,
							nullptr);
				if (rc >= 0)
				{
					pcre2_match_data_free(match_data);
					return { p.pattern, p.value };
				}
			}
		}
	}
	if (!state.empty())
	{
		auto x = bouncelist->default_grouping_state_pattern.find(state);
		if (x != bouncelist->default_grouping_state_pattern.end())
		{
			for (const auto & p : x->second)
			{
				int rc = pcre2_match(p.pcre_,
							(PCRE2_SPTR)message.c_str(),
							message.size(),
							0,
							PCRE2_NO_UTF_CHECK,
							match_data,
							nullptr);
				if (rc >= 0)
				{
					pcre2_match_data_free(match_data);
					return { p.pattern, p.value };
				}
			}
		}
	}
	for (const auto & p : bouncelist->default_grouping_default_state_pattern)
	{
		int rc = pcre2_match(p.pcre_,
					(PCRE2_SPTR)message.c_str(),
					message.size(),
					0,
					PCRE2_NO_UTF_CHECK,
					match_data,
					nullptr);
		if (rc >= 0)
		{
			pcre2_match_data_free(match_data);
			return { p.pattern, p.value };
		}
	}

	pcre2_match_data_free(match_data);
	return {};
}

static void list_reopen(const std::string& list)
{
	listslock.lock();
	auto l = lists.find(list);
	if (l == lists.end())
	{
		listslock.unlock();
		throw std::runtime_error("No such list id");
	}
	auto currentbouncelist = l->second;
	listslock.unlock();

	auto bouncelist = std::make_shared<bouncepatterns>();
	bouncelist->path = currentbouncelist->path;
	bouncelist->autoreload = currentbouncelist->autoreload;

	list_parse(bouncelist->path, bouncelist);

	listslock.lock();
	lists[list] = bouncelist;
	listslock.unlock();
}
