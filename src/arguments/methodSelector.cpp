#include <iostream>

#include "methodSelector.hpp"

#include "../modes/AlphaEstimation.hpp"

#include "../methods/NoneMethod.hpp"
#include "../methods/GreedyLCCS.hpp"
#include "../methods/WeightedAlignmentVoter.hpp"
#include "../methods/LGraalWrapper.hpp"
#include "../methods/HubAlignWrapper.hpp"
#include "../methods/TabuSearch.hpp"
#include "../methods/HillClimbing.hpp"
#include "../methods/SANA.hpp"
#include "../methods/RandomAligner.hpp"

using namespace std;

const string scoreFile = "topologySequenceScoreTable.cnf";

vector<vector<string> > getScoreTable() {
    if (not fileExists(scoreFile)) {
        throw runtime_error("Couldn't find file "+scoreFile);
    }
    return fileToStringsByLines(scoreFile);
}

vector<string> getScoreTuple(string methodName, string G1Name, string G2Name) {
    vector<vector<string> > scoreTable = getScoreTable();
    for (vector<string> line : scoreTable) {
        if (methodName == line[0] and G1Name == line[1] and G2Name == line[2]) {
            return line;
        }
    }
    throw runtime_error("Couldn't find entry in "+scoreFile+" for "+methodName+" "+G1Name+" "+G2Name);
}

double getTopScore(string methodName, string G1Name, string G2Name) {
    vector<string> tuple = getScoreTuple(methodName, G1Name, G2Name);
    return stod(tuple[3]);
}

double getSeqScore(string methodName, string G1Name, string G2Name) {
    vector<string> tuple = getScoreTuple(methodName, G1Name, G2Name);
    return stod(tuple[4]);
}

double betaDerivedAlpha(string methodName, string G1Name, string G2Name, double beta) {
    double topScore = getTopScore(methodName, G1Name, G2Name);
    double seqScore = getSeqScore(methodName, G1Name, G2Name);

    double topFactor = beta*topScore;
    double seqFactor = (1-beta)*seqScore;
    return topFactor/(topFactor+seqFactor);
}

Method* initLgraal(Graph& G1, Graph& G2, ArgumentParser& args) {
    string objFunType = args.strings["-objfuntype"];

    double alpha;
    if (objFunType == "generic") {
        throw runtime_error("generic objective function not supported for L-GRAAL");
    } else if (objFunType == "alpha") {
        alpha = args.doubles["-alpha"];
    } else if (objFunType == "beta") {
        double beta = args.doubles["-beta"];
        alpha = betaDerivedAlpha("lgraal", G1.getName(), G2.getName(), beta);
    } else {
        throw runtime_error("unknown value of -objfuntype: "+objFunType);
    }

    double iters = args.doubles["-lgraaliter"];
    double seconds = args.doubles["-t"]*60;
    return new LGraalWrapper(&G1, &G2, alpha, iters, seconds);
}

Method* initHubAlign(Graph& G1, Graph& G2, ArgumentParser& args) {
    string objFunType = args.strings["-objfuntype"];

    double alpha;
    if (objFunType == "generic") {
        throw runtime_error("generic objective function not supported for HubAlign");
    } else if (objFunType == "alpha") {
        alpha = args.doubles["-alpha"];
    } else if (objFunType == "beta") {
        double beta = args.doubles["-beta"];
        alpha = betaDerivedAlpha("lgraal", G1.getName(), G2.getName(), beta);
    } else {
        throw runtime_error("unknown value of -objfuntype: "+objFunType);
    }

    //in hubalign alpha is the fraction of topology
    return new HubAlignWrapper(&G1, &G2, 1 - alpha);
}

// If the objective function type (-objfuntype) is not generic,
//the weights of the measures in M are adjusted to an alpha based weighting
void updateObjFun(string methodName, Graph& G1, Graph& G2, ArgumentParser& args, MeasureCombination& M) {
    string objFunType = args.strings["-objfuntype"];
    if (objFunType == "generic") {
        //nothing to do
    } else if (objFunType == "alpha" or objFunType == "beta") {
        string topMeasure = args.strings["-topmeasure"];
        if (topMeasure != "ec" and topMeasure != "s3" and topMeasure != "wec") {
            cerr << "Warning: -topmeasure is " << topMeasure << endl;
        }
        double alpha;
        if (objFunType == "alpha") {
            alpha = args.doubles["-alpha"];
        } else {
            string methodId = methodName + topMeasure;
            double beta = args.doubles["-beta"];
            alpha = betaDerivedAlpha(methodId, G1.getName(), G2.getName(), beta);
        }
        M.setAlphaBasedWeights(topMeasure, alpha);
    } else {
        throw runtime_error("unknown value of -objfuntype: "+objFunType);
    }

    cerr << "=== "+methodName+" -- optimize: ===" << endl;
    M.printWeights(cerr);
    cerr << endl;
}

Method* initTabuSearch(Graph& G1, Graph& G2, ArgumentParser& args, MeasureCombination& M) {

    updateObjFun("tabu", G1, G2, args, M);
    double minutes = args.doubles["-t"];
    uint ntabus = args.doubles["-ntabus"];
    uint nneighbors = args.doubles["-nneighbors"];
    bool nodeTabus = args.bools["-nodetabus"];
    return new TabuSearch(&G1, &G2, minutes, &M, ntabus, nneighbors, nodeTabus);
}

Method* initSANA(Graph& G1, Graph& G2, ArgumentParser& args, MeasureCombination& M) {

    updateObjFun("sana", G1, G2, args, M);

    double T_initial = 0;
    if (args.strings["-tinitial"] != "auto") {
        T_initial = stod(args.strings["-tinitial"]);
    }

    double T_decay = 0;
    if (args.strings["-tdecay"] != "auto") {
        T_decay = stod(args.strings["-tdecay"]);
    }

    double minutes = args.doubles["-t"];

    Method* sana = new SANA(&G1, &G2, T_initial, T_decay, minutes, &M);
    if (args.bools["-restart"]) {
        double tnew = args.doubles["-tnew"];
        uint iterperstep = args.doubles["-iterperstep"];
        uint numcand = args.doubles["-numcand"];
        double tcand = args.doubles["-tcand"];
        double tfin = args.doubles["-tfin"];
        ((SANA*) sana)->enableRestartScheme(tnew, iterperstep, numcand, tcand, tfin);
    }
    if (args.strings["-tinitial"] == "auto") {
        ((SANA*) sana)->setT_INITIALAutomatically();
    }
    if (args.strings["-tdecay"] == "auto") {
        ((SANA*) sana)->setT_DECAYAutomatically();
    }
    return sana;
}

Method* initMethod(Graph& G1, Graph& G2, ArgumentParser& args, MeasureCombination& M) {
 
    string aligFile = args.strings["-eval"];
    if (aligFile != "")
        return new NoneMethod(&G1, &G2, aligFile);

    string name = args.strings["-method"];
    string startAligName = args.strings["-startalignment"];

    if (name == "greedylccs")
        return new GreedyLCCS(&G1, &G2, startAligName);
    if (name == "wave") {
        LocalMeasure* waveNodeSim = 
            (LocalMeasure*) M.getMeasure(args.strings["-wavenodesim"]);
        return new WeightedAlignmentVoter(&G1, &G2, waveNodeSim);
    } 
    if (name == "lgraal")
        return initLgraal(G1, G2, args);
    if (name == "hubalign")
        return initHubAlign(G1, G2, args);
    if (name == "tabu")
        return initTabuSearch(G1, G2, args, M);
    if (name == "sana")
        return initSANA(G1, G2, args, M);
    if (name == "hc")
        return new HillClimbing(&G1, &G2, &M, startAligName);
    if (name == "random")
        return new RandomAligner(&G1, &G2);
    if (name == "none")
        return new NoneMethod(&G1, &G2, startAligName);

    throw runtime_error("Error: unknown method: " + name);
}
