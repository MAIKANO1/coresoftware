#ifndef bcocheck_H__
#define bcocheck_H__

// std headers
#include <vector>
#include <filesystem>
#include <array>
#include <iostream>
#include <iomanip> // setw, setfill

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <set>

// ROOT headers
#include <TObject.h>
#include <TSystem.h>
#include <TStyle.h>
#include <TFile.h>
#include <TTree.h>
#include <TH1.h>
#include <TCanvas.h>
#include <TPaveStats.h>
#include <TLine.h>
#include <TLegend.h>
#include <TH2.h>
#include <TROOT.h>
#include <TGraph.h>

// Fun4All headers
#include <fun4all/SubsysReco.h>

#include <fun4all/Fun4AllHistoManager.h>
#include <fun4all/Fun4AllReturnCodes.h>

#include <ffarawobjects/InttRawHit.h>
#include <ffarawobjects/InttRawHitContainer.h>

#include <trackbase/InttEventInfo.h>
#include <trackbase/InttEventInfov1.h>

#include <phool/PHCompositeNode.h>
#include <phool/getClass.h>


class PHCompositeNode;

class bcocheck : public SubsysReco {


 public:
  bcocheck(const std::string &name = "bcocheck", const int run_num=0,const int felix_num=0);

  virtual ~bcocheck();

  int Init(PHCompositeNode *);
  
  int InitRun(PHCompositeNode *);
  
  /// SubsysReco event processing method
  int process_event(PHCompositeNode *);

  /// SubsysReco end processing method
  int EndRun(PHCompositeNode *);

  int End(PHCompositeNode *);

  //int SetHistBin(std::string type);
 private:

  // general variables
  int run_num_ = 0;
  int felix_num_=0;
  static const int kFelix_num_ = 8; // the number of our FELIX server
  static const int kFee_num_ = 14;  // the number of half-ladders in a single FELIX server
  static const int kChip_num_ = 26; // the number of chip in a half-ladder
  static const int kChan_num_ = 128; // the number of channel in a single chip
  static const int kFirst_pid_ = 3001; // the first pid (packet ID), which means intt0
  static const int divimul=10;

  int ievent_ = 0;
  int n=kFelix_num_;
  TFile* tf_output_[kFelix_num_];

  TH1D *h_full_bco[kFelix_num_];

  void DrawHists();

};
#endif