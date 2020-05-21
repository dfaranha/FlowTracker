/*
 * parserXML.h
 *
 *  Created on: Apr 1, 2014
 *      Author: leonardoribeiro
 */

#ifndef PARSERXML_H_
#define PARSERXML_H_

#include <iostream>
#include "tinyxml2.h"
#include <vector>


namespace parsersXML {
class parserXML {
public:
	class funcaoFonte {
	public:
		std::string getName();
		void setNome(std::string nome);
		funcaoFonte();
		std::vector<std::string> parameters;
		std::vector<std::string> parameters_public;
	private:
		std::string nomeFuncao;

	};


	bool setFile(std::string fileName);
	std::vector<funcaoFonte> getFontes();
	parserXML(std::string fileName);
	parserXML();
	virtual ~parserXML();


private:
	std::vector<funcaoFonte> funcoes_fonte;
	tinyxml2::XMLDocument doc;

};

}
#endif /* PARSERXML_H_ */

