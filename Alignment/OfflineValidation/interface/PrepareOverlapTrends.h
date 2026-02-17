#ifndef ALIGNMENT_OFFLINEVALIDATION_PREPAREOVERLAPTRENDS_H_
#define ALIGNMENT_OFFLINEVALIDATION_PREPAREOVERLAPTRENDS_H_

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <iomanip>
#include <fstream>
#include <experimental/filesystem>
#include "TPad.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TMultiGraph.h"
#include "TH1.h"
#include "THStack.h"
#include "TROOT.h"
#include "TFile.h"
#include "TLegend.h"
#include "TLegendEntry.h"
#include "TMath.h"
#include "TRegexp.h"
#include "TPaveLabel.h"
#include "TPaveText.h"
#include "TStyle.h"
#include "TLine.h"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/json_parser.hpp"

/*!
 * \def Dummy value in case a Overlap would fail for instance
 */
#define DUMMY -999.


#define OverlapFactor 10000.

/*! \struct Point
 *  \brief Structure Point
 *         Contains parameters of Gaussian fits to Overlaps
 *  
 * @param run:             run number (IOV boundary)
 * @param scale:           scale for the measured quantity: cm->μm for Overlaps, 1 for normalized residuals
 * @param mu:              mu/mean from Gaussian fit to Overlap/DrmsNR
 * @param sigma:           sigma/standard deviation from Gaussian fit to Overlap/DrmsNR
 */

struct OverlapPoint {
  float run, scale, mu, sigma;


/* Constructor */

  OverlapPoint(float Run = DUMMY,
        float ScaleFactor = OverlapFactor,
        float y1 = DUMMY,
        float y2 = DUMMY)
      : run(Run), scale(ScaleFactor), mu(y1), sigma(y2) {}

/* Constructor with splits */

/* Point(float Run, float ScaleFactor, TH1 *histo) : Point(Run, ScaleFactor, histo->GetMean(), histo->GetMeanError()) {} */

  inline float GetRun() const { return run; }
  inline float GetMu() const { return scale * mu; }
  inline float GetSigma() const { return scale * sigma; }
 
};



/*! \class Geometry
 *  \brief Class Geometry
 *         Contains vector for fit parameters (mean, sigma, etc.) obtained from multiple IOVs
 *         See Structure Point for description of the parameters.
 */


class OverlapGeometry {
public:
  std::vector<OverlapPoint> points;

private:
  //template<typename T> std::vector<T> GetQuantity (T (Point::*getter)() const) const {
  std::vector<float> GetQuantity(float (OverlapPoint::*getter)() const) const {
    std::vector<float> v;
    for (OverlapPoint point : points) {
      float value = (point.*getter)();
      v.push_back(value);
    }
    return v;
  }

public:
  TString title;
  OverlapGeometry() : title("") {}
  OverlapGeometry(TString Title) : title(Title) {}

  inline void SetTitle(TString Title) { title = Title; }
  inline TString GetTitle() { return title; }
  inline std::vector<float> Run() const { return GetQuantity(&OverlapPoint::GetRun); }
  inline std::vector<float> Mu() const { return GetQuantity(&OverlapPoint::GetMu); }
  inline std::vector<float> Sigma() const { return GetQuantity(&OverlapPoint::GetSigma); }
};


class PrepareOverlapTrends {
public:
  PrepareOverlapTrends(const char *outputFileName, boost::property_tree::ptree &json);
  ~PrepareOverlapTrends() {}
  void setDirsAndLabels(boost::property_tree::ptree &json);

  TString getName(TString structure, int layer, TString geometry);
  void bookOverlapTrends(std::vector<int> IOVlist,
                        std::vector<std::string> inputFiles,
                        TString structure,
                        TString overlap,
                        bool FORCE = false);
  
  void WriteOverlapTrends();
  std::map<std::pair<std::pair<TString,TString>,TString>,OverlapGeometry> bookpoints; // <<strucutre, overlap>, <geometry>> points 

private:
  const char *outputFileName_;
  std::vector<std::string> DirList;
  std::vector<std::string> GeometryList;
  std::pair<float,float> ExtractGaussian(TFile* inputFile, std::string geometry);
  
};

#endif  // ALIGNMENT_OFFLINEVALIDATION_PREPAREOVERLAPTRENDS_H_
