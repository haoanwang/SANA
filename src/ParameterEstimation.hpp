#ifndef ParameterEstimation_H
#define ParameterEstimation_H
#include <vector>
#include <string>
#include "Measure.hpp"
#include "Method.hpp"
#include "Graph.hpp"

using namespace std;

class ParameterEstimation {
public:
	ParameterEstimation(string parameterEstimationFile);

	void submitScriptsToCluster();
	void collectData();
	void printData(string outputFile);
	//void printDataCSV(string outputFile);


private:
	string measureName;
	Measure* measure;
	Graph G1;
	Graph G2;

	vector<double> kValues;
	vector<double> lValues;

	string experimentFolder;

	string getScriptName(double k, double l);
	string getAlignmentFileName(double k, double l);

	void makeScript(double k, double l);
	void submitScript(double k, double l);

	double getScore(double k, double l);

	vector<vector<double> > data;

	static const int PRECISION_DECIMALS;
	static const double minutes;
	static const string method;
	static const string projectFolder;
};


#endif