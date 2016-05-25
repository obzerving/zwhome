/*
 * zwUtils.h
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#ifndef ZWUTILS_H_
#define ZWUTILS_H_

#include <vector>
#include <string>
#include "zwRules.h"

int makeList(string items, vector<string> *itemlist);
void tmcopy(struct tm *tmsrc, struct tm *tmdest);
bool is_number(const std::string& str);

#endif /* ZWUTILS_H_ */
