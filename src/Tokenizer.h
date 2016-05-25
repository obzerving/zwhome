/*
 * Tokenizer.h
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#ifndef TOKENIZER_H_
#define TOKENIZER_H_

#include <string>
using namespace std;

class Tokenizer
{
public:
	static const std::string DELIMITERS;
	Tokenizer(const std::string& str);
	Tokenizer(const std::string& str, const std::string& delimiters);
	bool NextToken();
	bool NextToken(const std::string& delimiters);
	const std::string GetToken();
	void Reset();
protected:
	size_t m_offset;
	const std::string m_string;
	std::string m_token;
	std::string m_delimiters;
};

#endif /* TOKENIZER_H_ */
