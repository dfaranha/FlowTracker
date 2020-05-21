/*
 * parserXML.cpp
 *
 *  Created on: Apr 1, 2014
 *      Author: leonardoribeiro
 */

#include "parserXML.h"


parsersXML::parserXML::parserXML() {
	// TODO Auto-generated constructor stub

}

parsersXML::parserXML::parserXML(std::string fileName) {
	const char * c = fileName.c_str();
	int loadOkay = doc.LoadFile(c);
	if (loadOkay == 0)
		std::cout << "Arquivo XML encontrado" << std::endl;
	else
		std::cout << "Nao foi possivel encontrar o arquivo XML" << std::endl;

}


bool parsersXML::parserXML::setFile(std::string fileName){
	const char * c = fileName.c_str();
		int loadOkay = doc.LoadFile(c);
		if (loadOkay == 0)
			return true;
		else
			return false;
}

std::vector<parsersXML::parserXML::funcaoFonte> parsersXML::parserXML::getFontes() {
	tinyxml2::XMLElement * el = doc.FirstChildElement("functions")->FirstChildElement("sources")->FirstChildElement("function");
	while (el) {
		tinyxml2::XMLElement * el1 = el->FirstChildElement("name");
		const char* title = el1->GetText();		

		parsersXML::parserXML::funcaoFonte *funcaoF = new parsersXML::parserXML::funcaoFonte();
		funcaoF->setNome(title);

		tinyxml2::XMLElement * ret = el->FirstChildElement("return");
		funcaoF->parameters.push_back(ret->GetText());

		tinyxml2::XMLElement * el2 = el->FirstChildElement("secret")->FirstChildElement("parameter");
		while (el2) {
			funcaoF->parameters.push_back(std::string(el2->GetText()));
			el2 = el2->NextSiblingElement("parameter");
		}
		
		tinyxml2::XMLElement * el3 = el->FirstChildElement("public")->FirstChildElement("parameter");
                while (el3) {
                        funcaoF->parameters_public.push_back(std::string(el3->GetText()));
                        el3 = el3->NextSiblingElement("parameter");
                }

		funcoes_fonte.push_back(*funcaoF);
		el = el->NextSiblingElement("function");
	}
	free(el);
	return funcoes_fonte;
}


parsersXML::parserXML::~parserXML() {
}



std::string parsersXML::parserXML::funcaoFonte::getName(){
	return nomeFuncao;
}

void parsersXML::parserXML::funcaoFonte::setNome(std::string nome){
	nomeFuncao = nome;		
}

parsersXML::parserXML::funcaoFonte::funcaoFonte(){
}


