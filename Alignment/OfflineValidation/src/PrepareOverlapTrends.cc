#include "Alignment/OfflineValidation/interface/PrepareOverlapTrends.h"
#include "TString.h"
#include "TPRegexp.h"
#include <regex>
using namespace std;
namespace fs = std::experimental::filesystem;
namespace ph = std::placeholders;  // for _1, _2, _3...
namespace pt = boost::property_tree;

PrepareOverlapTrends::PrepareOverlapTrends(const char *outputFileName, pt::ptree &json)
    : outputFileName_(outputFileName) {
  setDirsAndLabels(json);
}

void PrepareOverlapTrends::setDirsAndLabels(pt::ptree &json) {
  DirList.clear();
  GeometryList.clear();
  for (const auto &childTree : json) {
    DirList.push_back(childTree.first.c_str());
    GeometryList.push_back(childTree.second.get<std::string>("title"));
  }
}

/* Function to extract the mean and uncertainty from the root canvas*/

std::pair<float,float> PrepareOverlapTrends::ExtractGaussian(TFile* inputFile, string geometry){

  std::ofstream outfile1("log_extract.txt", std::ios::app);

  pair<float,float> result = {0,0};

  TString canvas = "c1";
  TCanvas *c1 = dynamic_cast<TCanvas *>(inputFile->Get(canvas)); 
  
  //outfile1 << inputFile->ls() << endl;

  auto PrimList = c1->GetListOfPrimitives();

  outfile1 << PrimList->GetEntries() << endl;

  TIter next(PrimList);  

  while (TObject *prim = next()){
    if (prim->InheritsFrom(TLegend::Class())){
      outfile1 << "Found Legend! " << endl;
      TLegend *leg = (TLegend*)prim;
      auto legprim = leg->GetListOfPrimitives();
      TIter nextLegend(legprim);
      while(TObject *entry = nextLegend()){
        TLegendEntry *legEntry = (TLegendEntry*)entry;
        TString legLabel((string)legEntry->GetLabel());
        if(legLabel.Contains((TString)(geometry))){
          // Do Regex Stuff //
          std::regex re("([0-9.]*)#pm([0-9.]*)#mum");
          //std::regex re("([0-9]*)");
          std::smatch matches;
          string legString = (std::string)legLabel;
          if (std::regex_search(legString, matches, re)){
            result.first = stof(matches[1]);
            result.second = stof(matches[2]);
          }
        }
      } 
    }
  }

  return result;
}

void PrepareOverlapTrends::bookOverlapTrends(vector<int> IOVlist,
                                        std::vector<std::string> inputFiles,
                                        TString structure,
                                        TString overlap,
                                        bool FORCE) {
    gROOT->SetBatch();

    ROOT::EnableThreadSafety();
    TH1::AddDirectory(kFALSE);

    std::ofstream outfile("log.txt", std::ios::app);
    
    for (const auto &iov : IOVlist){
        outfile << iov << endl;
    }

    float ScaleFactor = OverlapFactor;

    TFile *f = nullptr;
    pair<float,float> Gaussian = {0,0};
    OverlapPoint *point = nullptr;
    

    /* Loop over all input files and add results to each geometry*/
    for (unsigned int i = 0; i < inputFiles.size(); ++i) {
      outfile << "Iterating " << inputFiles.at(i).c_str() << endl;
      int runN = IOVlist.at(i);
      for (const string &geometry : GeometryList){

        if (fs::is_empty(inputFiles.at(i).c_str())) {
          cout << "ERROR: Empty file " << inputFiles.at(i).c_str() << endl;
          if (FORCE){
            point = new OverlapPoint(runN,ScaleFactor,0,0);
          }
          continue;
        }
        else{
          /*  */      
          outfile << "EXTRACTING " << inputFiles.at(i).c_str() << "for " << geometry << endl;
          f = TFile::Open(inputFiles.at(i).c_str(), "READ");
          Gaussian = ExtractGaussian(f,geometry);
          outfile << Gaussian.first << " " << Gaussian.second << endl;
          point = new OverlapPoint(runN,ScaleFactor,Gaussian.first,Gaussian.second);
          f->Close();
        }
        bookpoints[make_pair(make_pair(structure,overlap),geometry)].points.push_back(*point);
      }
    }
  return;

}

void PrepareOverlapTrends::WriteOverlapTrends(){

  /* Perpare the output file and load Tgraphs*/
  TFile *fout = TFile::Open(outputFileName_, "RECREATE");
  TGraphErrors *g = nullptr;

  /* Load the strucutres and overlaps from the points and */

  vector<pair<TString,TString>> StructureAndOverlap;
  TString overlap;
  TString structure;

  for (auto const& x : bookpoints){
    StructureAndOverlap.push_back(x.first.first);
  }

  for (pair<TString,TString> &resultsToPlot: StructureAndOverlap){
      structure = resultsToPlot.first;
      overlap = resultsToPlot.second;
  /* Load the Tgraphs */
      for (const string &geometry : GeometryList){
        
        TString name = overlap + "_" + structure;
        OverlapGeometry geom = bookpoints[make_pair(make_pair(structure, overlap), geometry)];

        using Trend = vector<float> (OverlapGeometry::*)() const;
        vector<Trend> trends{&OverlapGeometry::Mu,
                             &OverlapGeometry::Sigma
        };

        vector<TString> variables{"mu","sigma"};
        vector<float> runs = geom.Run();
        size_t n = runs.size();
        vector<float> emptyvec;
        for (size_t i = 0; i < runs.size(); i++)
          emptyvec.push_back(0.);
        for (size_t iVar = 0; iVar < variables.size(); iVar++) {
          Trend trend = trends.at(iVar);
          g = new TGraphErrors(n, runs.data(), (geom.*trend)().data(), emptyvec.data(), emptyvec.data());
          g->SetTitle(geometry.c_str());
          g->Write(name + "_" + variables.at(iVar));
        }
        /* Only using this loop in case we expand the scope later*/
        vector<pair<Trend, Trend>> trendspair{make_pair(&OverlapGeometry::Mu, &OverlapGeometry::Sigma)};
        vector<pair<TString, TString>> variablepairs{make_pair("mu", "sigma")};
        for (size_t iVar = 0; iVar < variablepairs.size(); iVar++) {
          Trend meantrend = trendspair.at(iVar).first;
          Trend sigmatrend = trendspair.at(iVar).second;
          g = new TGraphErrors(
              n, runs.data(), (geom.*meantrend)().data(), emptyvec.data(), (geom.*sigmatrend)().data());
          g->SetTitle(geometry.c_str());
          TString graphname = name + "_" + variablepairs.at(iVar).first;
          graphname += "_" + variablepairs.at(iVar).second;
          g->Write(graphname);
        }
      
    }
  
  }
  fout->Close();
}