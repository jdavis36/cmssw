#include <cstdlib>
#include <string>
#include <tuple>
#include <iostream>
#include <numeric>
#include <functional>
#include <unistd.h>

#include "TFile.h"
#include "TGraph.h"
#include "TH1.h"

#include "exceptions.h"
#include "toolbox.h"
#include "Options.h"

#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "boost/filesystem.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/json_parser.hpp"
#include "boost/optional.hpp"

#include "TString.h"
#include "TColor.h"

#include "Alignment/OfflineValidation/interface/PrepareOverlapTrends.h"
#include "Alignment/OfflineValidation/interface/Trend.h"

using namespace std;
using namespace AllInOneConfig;
namespace fs = boost::filesystem;
namespace bc = boost::container;

static const char *bold = "\e[1m", *normal = "\e[0m";
static const float defaultConvertScale = 1000.;
static const int startRun2016 = 272930;
static const int endRun2018 = 325175;

namespace pt = boost::property_tree;

int trends(int argc, char *argv[]) {

  // parse the command line

  Options options;
  options.helper(argc, argv);
  options.parser(argc, argv);

  //Read in AllInOne json config
  pt::ptree main_tree;
  pt::read_json(options.config, main_tree);

  pt::ptree alignments = main_tree.get_child("alignments");
  pt::ptree validation = main_tree.get_child("validation");
  pt::ptree style = main_tree.get_child("style");

  //Read all configure variables and set default for missing keys
  string outputdir = main_tree.get<string>("output");

  bool FORCE = validation.count("FORCE") ? validation.get<bool>("FORCE") : false;
  string year = validation.count("year") ? validation.get<string>("year") : "Run2";
  TString lumiInputFile = style.get_child("trends").count("lumiInputFile")
                              ? style.get_child("trends").get<string>("lumiInputFile")
                              : "Alignment/OfflineValidation/data/lumiPerRun_Run2.txt";

  fs::path lumiFile = lumiInputFile.Data();
  edm::FileInPath fip = edm::FileInPath(lumiFile.string());
  fs::path pathToLumiFile = "";
  if (!fs::exists(lumiFile)) {
    pathToLumiFile = fip.fullPath();
  } else {
    pathToLumiFile = lumiFile;
  }
  if (!fs::exists(pathToLumiFile)) {
    cout << "ERROR: lumi-per-run file (" << lumiFile.string().data() << ") not found!" << endl
         << "Please check!" << endl;
    exit(EXIT_FAILURE);
  } else {
    cout << "Found lumi-per-run file: " << pathToLumiFile.string().data() << endl;
  }
  if (!lumiInputFile.Contains(year)) {
    cout << "ERROR: lumi-per-run file must contain (" << year.data() << ")!" << endl << "Please check!" << endl;
    exit(EXIT_FAILURE);
  }

  string lumiAxisType = "recorded";
  if (lumiInputFile.Contains("delivered"))
    lumiAxisType = "delivered";

  std::cout << Form("NOTE: using %s luminosity!", lumiAxisType.data()) << std::endl;

  vector<int> IOVlist;
  for (auto const &childTree : validation.get_child("IOV")) {
    int iov = childTree.second.get_value<int>();
    IOVlist.push_back(iov);
  }

  string labels_to_add = "";
  if (validation.count("labels")) {
    for (auto const &label : validation.get_child("labels")) {
      labels_to_add += "_";
      labels_to_add += label.second.get_value<string>();
    }
  }

  /* Initialize Overlap Trend Class*/

  fs::path pname = Form("%s/Overlaptrends%s.root", outputdir.data(), labels_to_add.data());
  PrepareOverlapTrends prepareTrends(pname.c_str(),alignments);

  /* Initialize what is need for trends*/

  vector<TString> structures = {"BPIX","FPIX","TIB","TID","TOB","TEC"}; 

  map<TString,vector<TString>> StructOverlaps{{"BPIX",{"phi_phi","phi_z","z_phi","z_z"}},{"FPIX",{"phi_phi","phi_r","r_phi","r_r"}},{"TIB",{"phi_phi","z_phi"}},{"TID",{"phi_phi","r_phi"}},{"TOB",{"phi_phi","z_phi"}},{"TEC",{"phi_phi","r_phi"}}};

  map<TString,TString,vector<pair<TString,pair<TString, vector<double>>>>> results; /* <Stucture, Overlap, <geometry, <IOV, results>>>*/
  vector<string> inputFiles;

  vector<pair<TString,TString>> allowedPairs;

  for (const auto &structure : structures) {
    TString structname = structure;
    for (const auto &overlap : StructOverlaps.at(structname)){
      TString overlapname = overlap;
      for (const auto &iov : IOVlist){
        TString mergeFile = validation.get<string>("mergeFile");
        string input = Form("%s%s/Residuals/%s_%s.root", mergeFile.Data(),to_string(iov).data(),overlap.Data(),structure.Data());
        inputFiles.push_back(input);
      }
      allowedPairs.push_back(make_pair(structure,overlap));
      /* Add the results to the final structure */
      prepareTrends.bookOverlapTrends(IOVlist,inputFiles,structure,overlap,FORCE);
      inputFiles.clear();
    }
  }
  /* After booking results add the graphs to TFile and dump out*/
  prepareTrends.WriteOverlapTrends();

  /* Setup various required inputs */
  float convertUnit = style.get_child("trends").count("convertUnit")
                          ? style.get_child("trends").get<float>("convertUnit")
                          : defaultConvertScale;
  int firstRun = validation.count("firstRun") ? validation.get<int>("firstRun") : startRun2016;
  int lastRun = validation.count("lastRun") ? validation.get<int>("lastRun") : endRun2018;
  const Run2Lumi GetLumi(pathToLumiFile.string().data(), firstRun, lastRun, convertUnit);

  /* Open the TFile and make the Trends */

  map<TString,tuple<TString,float,float>> Overlaps = {{"phi_phi",{"<#delta_{#phi}>_{#phi} [#mum]",-6,12}},
                                 {"phi_z",{"<#delta_{#phi}>_{z} [#mum]",-30,30}},
                                 {"z_phi",{"<#delta_{z}>_{#phi} [#mum]",-30,30}},
                                 {"z_z",{"<#delta_{z}>_{z} [#mum]",-30,30}},
                                 {"r_phi",{"<#delta_{r}>_{#phi} [#mum]",-30,30}},
                                 {"phi_r",{"<#delta_{#phi}>_{r} [#mum]",-30,30}},
                                 {"r_r",{"<#delta_{r}>_{r} [#mum]",-30,30}}};

  auto f = TFile::Open(pname.c_str());

  for (const auto &p : allowedPairs){
    TString overlap = p.second;
    TString name = p.first+"_"+p.second; 
    tuple<TString,float,float> plotInfo = Overlaps[overlap];

    TString ytitle = get<0>(plotInfo);
    float ymin = get<1>(plotInfo), ymax = get<2>(plotInfo);

    Trend trend(name,
                      outputdir.data(),
                      ytitle,
                      ytitle,
                      ymin,
                      ymax,
                      style,
                      GetLumi,
                      lumiAxisType.data());
    for (auto const &alignment : alignments) {
      bool fullRange = true;
      if (style.get_child("trends").count("earlyStops")) {
        for (auto const &earlyStop : style.get_child("trends.earlyStops")) {
          if (earlyStop.second.get_value<string>() == alignment.first)
            fullRange = false;
        }
      }

      TString gtitle = alignment.second.get<string>("title");
      TString gname = Form("%s_%s_%s_%s",p.second.Data(),p.first.Data(),"mu","sigma");
      auto g = Get<TGraphErrors>(gname);
      assert(g != nullptr);
      g->SetTitle(gtitle);
      g->SetMarkerSize(0.6);
      int color = alignment.second.get<int>("color");
      int style = floor(alignment.second.get<double>("style") /100.);
      g->SetFillColorAlpha(color,0.2);
      g->SetMarkerColor(color);
      g->SetMarkerStyle(style);
      g->SetLineColor(kWhite);
      trend(g,"P2","pf",fullRange);
    }
  }

  f->Close();
  
  cout << bold << "Done" << normal << endl;

  return EXIT_SUCCESS;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
int main(int argc, char *argv[]) { return exceptions<trends>(argc, argv); }
#endif
