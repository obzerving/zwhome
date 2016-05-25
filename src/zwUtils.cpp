/*
 * zwUtils.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include "stdafx.h"
#include "Manager.h"
#include <cctype>
#include <ctime>
#include "Tokenizer.h"
#include "zwUtils.h"

using namespace OpenZWave;
using namespace hdate;

int makeList(string items, vector<string> *itemlist)
{
	Tokenizer atoken(items);
	while(atoken.NextToken("|\0"))
	{
		itemlist->push_back(atoken.GetToken());
	};
	return 0;
};

bool is_number(const std::string& str)
{
	unsigned int i;
	for (i = 0; i < str.length(); i++)
	{
		if (!std::isdigit(str[i]) && str[i] != '.')
		{
			return false;
		}
	};
	return true;
};

void tmcopy(struct tm *tmsrc, struct tm *tmdest)
{
	tmdest->tm_hour = tmsrc->tm_hour;
	tmdest->tm_isdst = tmsrc->tm_isdst;
	tmdest->tm_mday = tmsrc->tm_mday;
	tmdest->tm_min = tmsrc->tm_min;
	tmdest->tm_mon = tmsrc->tm_mon;
	tmdest->tm_sec = tmsrc->tm_sec;
	tmdest->tm_wday = tmsrc->tm_wday;
	tmdest->tm_yday = tmsrc->tm_yday;
	tmdest->tm_year = tmsrc->tm_year;
};
