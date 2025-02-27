// vim: ts=2 sw=2 et
/*
 * Gamma Combination
 * Author: Maximilian Schlupp, maxschlupp@gmail.com
 * Author: Konstantin Schubert, schubert.konstantin@gmail.com
 * Date: October 2016
 *
 *
 * The term "free fit do data" refers to the fit which is performed to data, where the parameter
 * of interest is left to float freely.
 *
 * The term "constrained fit to data" refers to the fit which is performed to data, where the
 * parameter of interest is fixed to a certain value (scanpoint)
 */

#include "MethodDatasetsPluginScan.h"
#include "TRandom3.h"
#include "TArrow.h"
#include "TLatex.h"
#include <algorithm>
#include <ios>
#include <iomanip>



///
/// The default constructor for the dataset plugin scan
///
MethodDatasetsPluginScan::MethodDatasetsPluginScan(MethodProbScan* probScan, PDF_Datasets* PDF, OptParser* opt):
    MethodPluginScan(probScan, PDF, opt),
    pdf                 (PDF),
    drawPlots           (false),
    explicitInputFile   (false),
    dataFreeFitResult   (NULL)
{
    chi2minGlobalFound = true; // the free fit to data must be done and must be saved to the workspace before gammacombo is even called
    methodName = "DatasetsPlugin";
    w = PDF->getWorkspace();
    title = PDF->getTitle();
    name =  PDF->getName();

    if ( arg->var.size() > 1 ) scanVar2 = arg->var[1];
    inputFiles.clear();


    if (w->obj("data_fit_result") == NULL) { //\todo: support passing the name of the fit result in the workspace.
        cerr << "ERROR: The workspace must contain the fit result of the fit to data. The name of the fit result must be 'data_fit_result'. " << endl;
        exit(EXIT_FAILURE);
    }
    dataFreeFitResult = (RooFitResult*) w->obj("data_fit_result");
    // chi2minGlobal = 2 * dataFreeFitResult->minNll();
    chi2minGlobal = probScan->getChi2minGlobal();
    std::cout << "=============== Global Minimum (2*-Log(Likelihood)) is: 2*" << dataFreeFitResult->minNll() << " = " << chi2minGlobal << endl;

    chi2minBkg = probScan->getChi2minBkg();
    std::cout << "=============== Bkg minimum (2*-Log(Likelihood)) is: " << chi2minBkg << endl;
    if (chi2minBkg<chi2minGlobal)
    {
        std::cout << "WARNING: BKG MINIMUM IS LOWER THAN GLOBAL MINIMUM! The likelihoods are screwed up! Set bkg minimum to global minimum for consistency." << std::endl;
        chi2minBkg = chi2minGlobal;
        std::cout << "=============== New bkg minimum (2*-Log(Likelihood)) is: " << chi2minBkg << endl;
    }

    if ( !w->set(pdf->getObsName()) ) {
        cerr << "MethodDatasetsPluginScan::MethodDatasetsPluginScan() : ERROR : no '" + pdf->getObsName() + "' set found in workspace" << endl;
        cerr << " You can specify the name of the set in the workspace using the pdf->initObservables(..) method.";
        exit(EXIT_FAILURE);
    }
    if ( !w->set(pdf->getParName()) ) {
        cerr << "MethodDatasetsPluginScan::MethodDatasetsPluginScan() : ERROR : no '" + pdf->getParName() + "' set found in workspace" << endl;
        exit(EXIT_FAILURE);
    }
}

///////////////////////////////////////////////
///
/// Gets values of certain parameters as they were at the given scanpoint-index after the constrained fit to data
///
///////////////////////////////////////////////
float MethodDatasetsPluginScan::getParValAtIndex(int index, TString parName) {

    this->getProfileLH()->probScanTree->t->GetEntry(index);
    TLeaf* var = this->getProfileLH()->probScanTree->t->GetLeaf(parName); //<- pretty sure that this will give a segfault, we need to use parName + "_scan"
    if (!var) {
        cout << "MethodDatasetsPluginScan::getParValAtScanpoint() : ERROR : variable (" << parName << ") not found!" << endl;
        exit(EXIT_FAILURE);
    }
    return var->GetValue();
}


void MethodDatasetsPluginScan::initScan() {
    if ( arg->debug ) cout << "MethodDatasetsPluginScan::initScan() : initializing ..." << endl;

    // Init the 1-CL histograms. Range is taken from the scan range, unless
    // the --scanrange command line argument is set.
    RooRealVar *par1 = w->var(scanVar1);
    if ( !par1 ) {
        cout << "MethodDatasetsPluginScan::initScan() : ERROR : No such scan parameter: " << scanVar1 << endl;
        cout << "MethodDatasetsPluginScan::initScan() :         Choose an existing one using: --var par" << endl << endl;
        cout << "  Available parameters:" << endl << "  ---------------------" << endl << endl << "  ";
        pdf->printParameters();
        exit(EXIT_FAILURE);
    }
    if ( arg->scanrangeMin != arg->scanrangeMax ) par1->setRange("scan", arg->scanrangeMin, arg->scanrangeMax);
    Utils::setLimit(w, scanVar1, "scan");

    if (hCL) delete hCL;
    hCL = new TH1F("hCL" + getUniqueRootName(), "hCL" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if (hCLs) delete hCLs;
    hCLs = new TH1F("hCLs" + getUniqueRootName(), "hCLs" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if (hCLsFreq) delete hCLsFreq;
    hCLsFreq = new TH1F("hCLsFreq" + getUniqueRootName(), "hCLsFreq" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if (hCLsExp) delete hCLsExp;
    hCLsExp = new TH1F("hCLsExp" + getUniqueRootName(), "hCLsExp" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if (hCLsErr1Up) delete hCLsErr1Up;
    hCLsErr1Up = new TH1F("hCLsErr1Up" + getUniqueRootName(), "hCLsErr1Up" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if (hCLsErr1Dn) delete hCLsErr1Dn;
    hCLsErr1Dn = new TH1F("hCLsErr1Dn" + getUniqueRootName(), "hCLsErr1Dn" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if (hCLsErr2Up) delete hCLsErr2Up;
    hCLsErr2Up = new TH1F("hCLsErr2Up" + getUniqueRootName(), "hCLsErr2Up" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if (hCLsErr2Dn) delete hCLsErr2Dn;
    hCLsErr2Dn = new TH1F("hCLsErr2Dn" + getUniqueRootName(), "hCLsErr2Dn" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());
    if ( hChi2min ) delete hChi2min;
    hChi2min = new TH1F("hChi2min" + getUniqueRootName(), "hChi2min" + pdf->getPdfName(), nPoints1d, par1->getMin(), par1->getMax());

    // fill the chi2 histogram with very unlikely values such
    // that inside scan1d() the if clauses work correctly
    for ( int i = 1; i <= nPoints1d; i++ ) hChi2min->SetBinContent(i, 1e6);

    if ( scanVar2 != "" ) {
        cout << "MethodDatasetsPluginScan::initScan(): EROR: Scanning in more than one dimension is not supported." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Set up storage for the fit results.
    // Clear before so we can call initScan() multiple times.
    // Note that allResults still needs to hold all results, so don't delete the RooFitResults.

    curveResults.clear();
    for ( int i = 0; i < nPoints1d; i++ ) curveResults.push_back(0);

    // turn off some messages
    RooMsgService::instance().setStreamStatus(0, kFALSE);
    RooMsgService::instance().setStreamStatus(1, kFALSE);
    if (arg->debug) {
        std::cout << "DEBUG in MethodDatasetsPluginScan::initScan() - Scan initialized successfully!\n" << std::endl;
    }
    this->checkExtProfileLH();
}

//////////////////////////////////////////////
///
/// This checks if the TTree which originated from a previous prob scan
/// for compatibility with the current scan: Did it use the same number
/// of scan points, did it use the same scan range?
/// If everything is fine, keeps a pointer to the tree in this->probScanTree
///
//////////////////////////////////////////////
void MethodDatasetsPluginScan::checkExtProfileLH() {

    TTree* tree = this->getProfileLH()->probScanTree->t;

    //make sure that the scan points in the tree match number
    //of scan points and the scan range that we are using now.
    TBranch* b    = (TBranch*)tree->GetBranch("scanpoint");
    int entriesInTree = b->GetEntries();
    if (nPoints1d != entriesInTree) {
        std::cout << "Number of scan points in tree saved from prob scan do not match number of scan points used in plugin scan." << std::endl;
        exit(EXIT_FAILURE);
    }


    float parameterToScan_min = hCL->GetXaxis()->GetXmin();
    float parameterToScan_max = hCL->GetXaxis()->GetXmax();

    tree->GetEntry(0);
    float minTreePoint = b->GetLeaf("scanpoint")->GetValue();
    if ((minTreePoint - parameterToScan_min) / std::max(parameterToScan_max, parameterToScan_min) > 1e-5) {
        std::cout << "Lowest scan point in tree saved from prob scan does not match lowest scan point used in plugin scan." << std::endl;
        std::cout << "Alternatively, this could be a problem with the heuristics used for checking the equality of two floats" << std::endl;
        exit(EXIT_FAILURE);
    }

    tree->GetEntry(entriesInTree - 1);
    float maxTreePoint = b->GetLeaf("scanpoint")->GetValue();
    if ((maxTreePoint - parameterToScan_max) / std::max(parameterToScan_max, parameterToScan_min) > 1e-5) {
        std::cout << "Max scan point in tree saved from prob scan probably does not match max scan point used in plugin scan." << std::endl;
        std::cout << "Alternatively, this could be a problem with the heuristics used for checking the equality of two floats" << std::endl;
        exit(EXIT_FAILURE);
    }
};

///////////////////////////////////////////////////
///
/// Prepare environment for toy fit
///
/// \param pdf      the pdf that is to be fitted.
///
////////////////////////////////////////////////////
RooFitResult* MethodDatasetsPluginScan::loadAndFit(PDF_Datasets* pdf) {
    // we want to fit to the latest simulated toys
    // first, try to simulated toy values of the global observables from a snapshot
    if (!w->loadSnapshot(pdf->globalObsToySnapshotName)) {
        std::cout << "FATAL in MethodDatasetsPluginScan::loadAndFit() - No snapshot globalObsToySnapshotName found!\n" << std::endl;
        exit(EXIT_FAILURE);
    };
    // then, fit the pdf while passing it the simulated toy dataset
    return pdf->fit(pdf->getToyObservables());
};

///
/// load Parameter limits
/// by default the "free" limit is loaded, can be changed to "phys" by command line argument
///
void MethodDatasetsPluginScan::loadParameterLimits() {
    TString rangeName = arg->enforcePhysRange ? "phys" : "free";
    if ( arg->debug ) cout << "DEBUG in Combiner::loadParameterLimits() : loading parameter ranges: " << rangeName << endl;
    TIterator* it = w->set(pdf->getParName())->createIterator();
    while ( RooRealVar* p = (RooRealVar*)it->Next() ) setLimit(w, p->GetName(), rangeName);
    delete it;
}


///
/// Print settings member of MethodDatasetsPluginScan
///
void MethodDatasetsPluginScan::print() {
    cout << "########################## Print MethodDatasetsPluginScan Class ##########################" << endl;
    cout << "\t --- " << "Method Name: \t\t\t" << methodName << endl;
    cout << "\t --- " << "Instance Name: \t\t\t" << name << endl;
    cout << "\t --- " << "Instance Title: \t\t\t" << title << endl;
    cout << "\t --- " << "Scan Var Name: \t\t\t" << scanVar1 << endl;
    if ( arg->var.size() > 1 )   cout << "\t --- " << "2nd Scan Var Name: \t\t" << scanVar2 << endl;
    cout << "\t --- " << "Number of Scanpoints 1D: \t\t" << nPoints1d << endl;
    cout << "\t --- " << "Number of Scanpoints x 2D: \t" << nPoints2dx << endl;
    cout << "\t --- " << "Number of Scanpoints y 2D: \t" << nPoints2dy << endl;
    cout << "\t --- " << "Number of Toys per scanpoint: \t" << nToys << endl;
    cout << "\t --- " << "PDF Name: \t\t\t\t" << pdf->getPdfName() << endl;
    cout << "\t --- " << "Observables Name: \t\t\t" <<  pdf->getObsName() << endl;
    cout << "\t --- " << "Parameters Name: \t\t\t" <<  pdf->getParName() << endl;
    cout << "\t --- " << "Global minimum Chi2: \t\t" << chi2minGlobal << endl;
    cout << "\t --- " << "nrun: \t\t\t\t" << arg->nrun << endl;
    cout << "---------------------------------" << endl;
    cout << "\t --- Scan Var " << scanVar1 << " from " << getScanVar1()->getMin("scan")
         << " to " << getScanVar1()->getMax("scan") << endl;
    cout << "---------------------------------" << endl;
}


///
/// Compute the p-value at a certain point in parameter space using
/// the plugin method. The precision of the p-value will depend on
/// the number of toys that were generated, more than 100 should
/// be a good start (ntoys command line option).
///
/// \param runMin   defines lowest run number of toy jobs to read in
///
/// \param runMax   defines highest run number of toy jobs to read in
TChain* MethodDatasetsPluginScan::readFiles(int runMin, int runMax, int &nFilesRead, int &nFilesMissing, TString fileNameBaseIn) {
///
    TChain *c = new TChain("plugin");
    int _nFilesRead = 0;

    TString dirname = "root/scan1dDatasetsPlugin_" + this->pdf->getName() + "_" + scanVar1;
    TString fileNameBase = (fileNameBaseIn.EqualTo("default")) ? dirname + "/scan1dDatasetsPlugin_" + this->pdf->getName() + "_" + scanVar1 + "_run" : fileNameBaseIn;

    if (explicitInputFile) {
        for (TString &file : inputFiles) {
            Utils::assertFileExists(file);
            c->Add(file);
            _nFilesRead += 1;
        }
    }
    else {
        for (int i = runMin; i <= runMax; i++) {
            TString file = Form(fileNameBase + "%i.root", i);
            cout << "MethodDatasetsPluginScan::readFiles() : opening " << file << "\r";
            Utils::assertFileExists(file);
            c->Add(file);
            _nFilesRead += 1;
        }
    }

    nFilesRead = _nFilesRead;
    if ( nFilesRead == 0 ) {
        cout << "MethodDatasetsPluginScan::readFiles() : no files read!" << endl;
        exit(EXIT_FAILURE);
    }
    cout << "MethodDatasetsPluginScan::readFiles() : read files: " << nFilesRead << endl;
    return c;
}

//////////
/// readScan1dTrees implements inherited method
/// \param runMin   defines lowest run number of toy jobs to read in
///
/// \param runMax   defines highest run number of toy jobs to read in
///
/// \param fileNameBaseIn    optional, define the directory from which the files are to be read
///
/// This is the Plugin version of the method, there is also a version of the method for the prob scan, with _prob suffix.
/// \todo: Like for the normal gammacombo stuff, use a seperate class for the PROB scan! This is MethodDatasetsPLUGINScan.cpp, after all
/////////////
void MethodDatasetsPluginScan::readScan1dTrees(int runMin, int runMax, TString fileNameBaseIn)
{
		int nFilesRead, nFilesMissing;
    TChain* c = this->readFiles(runMin, runMax, nFilesRead, nFilesMissing, fileNameBaseIn);
    ToyTree t(this->pdf, this->arg, c);
    t.open();

    float halfBinWidth = (t.getScanpointMax() - t.getScanpointMin()) / ((float)t.getScanpointN()) / 2; //-1.)/2;
    /// \todo replace this such that there's always one bin per scan point, but still the range is the scan range.
    /// \todo Also, if we use the min/max from the tree, we have the problem that they are not exactly
    /// the scan range, so that the axis won't show the lowest and highest number.
    /// \todo If the scan range was changed after the toys were generate, we absolutely have
    /// to derive the range from the root files - else we'll have bining effects.
    delete hCL;
    hCL = new TH1F("hCL", "hCL", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);
    if (arg->debug) {
        printf("DEBUG %i %f %f %f\n", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth, halfBinWidth);
    }
    delete hCLs;
    hCLs = new TH1F("hCLs", "hCLs", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);
    if (arg->debug) {
        printf("DEBUG %i %f %f %f\n", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth, halfBinWidth);
    }
    delete hCLsFreq;
    hCLsFreq = new TH1F("hCLsFreq", "hCLs", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth );
    if (arg->debug) {
        printf("DEBUG %i %f %f %f\n", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth, halfBinWidth);
    }
    delete hCLsExp;
    hCLsExp = new TH1F("hCLsExp", "hCLs", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);
    delete hCLsErr1Up;
    hCLsErr1Up = new TH1F("hCLsErr1Up", "hCLs", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);
    delete hCLsErr1Dn;
    hCLsErr1Dn = new TH1F("hCLsErr1Dn", "hCLs", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);
    delete hCLsErr2Up;
    hCLsErr2Up = new TH1F("hCLsErr2Up", "hCLs", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);
    delete hCLsErr2Dn;
    hCLsErr2Dn = new TH1F("hCLsErr2Dn", "hCLs", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);
    delete hChi2min;
    hChi2min = new TH1F("hChi2min", "hChi2min", t.getScanpointN(), t.getScanpointMin() - halfBinWidth, t.getScanpointMax() + halfBinWidth);


    // histogram to store number of toys which enter p Value calculation
    TH1F *h_better        = (TH1F*)hCL->Clone("h_better");
    // histogram to store number of toys which enter CLs p Value calculation
    TH1F *h_better_cls        = (TH1F*)hCL->Clone("h_better_cls");
    // numbers for all toys
    TH1F *h_all           = (TH1F*)hCL->Clone("h_all");
    // numbers of toys failing the selection criteria
    TH1F *h_failed        = (TH1F*)hCL->Clone("h_failed");
    // numbers of toys which are not in the physical region dChi2<0
    TH1F *h_background    = (TH1F*)hCL->Clone("h_background");
    // histo for GoF test
    TH1F *h_gof           = (TH1F*)hCL->Clone("h_gof");
    // likelihood scan p values
    TH1F *h_probPValues   = (TH1F*)hCL->Clone("h_probPValues");
    // total number of toys
    TH1F *h_tot           = (TH1F*)hCL->Clone("h_tot");
    // histogram illustrating the failure rate
    TH1F *h_fracGoodToys  = (TH1F*)hCL->Clone("h_fracGoodToys");
    // map of vectors for CLb quantiles
    std::map<int,std::vector<double> > sampledBValues;
    std::map<int,std::vector<double> > sampledSBValues;
    TH1F *h_pVals         = new TH1F("p", "p", 200, 0.0, 1e-2);
    Long64_t nentries     = t.GetEntries();
    cout << "MethodDatasetsPluginScan::readScan1dTrees() : average number of toys per scanpoint: " << (double) nentries / (double)nPoints1d << endl;
    Long64_t nfailed      = 0;
    Long64_t nwrongrun    = 0;
    Long64_t n0better     = 0;
    Long64_t n0all        = 0;
    Long64_t n0tot        = 0;
    Long64_t n0failed     = 0;
    Long64_t totFailed    = 0;


    float printFreq = nentries > 101 ? 100 : nentries;  ///< for the status bar
    t.activateAllBranches();
    for (Long64_t i = 0; i < nentries; i++)
    {
        // status bar
        if (((int)i % (int)(nentries / printFreq)) == 0)
            cout << "MethodDatasetsPluginScan::readScan1dTrees() : reading entries "
                 << Form("%.0f", (float)i / (float)nentries * 100.) << "%   \r" << flush;
        // load entry
        t.GetEntry(i);

        bool valid    = true;

        h_tot->Fill(t.scanpoint);
        if (t.scanpoint == 0.0) n0tot++;
        // criteria for GammaCombo
        bool convergedFits      = (t.statusFree == 0. && t.statusScan == 0.);
        bool tooHighLikelihood  = !( abs(t.chi2minToy) < 1e27 && abs(t.chi2minGlobalToy) < 1e27);

        // apply cuts
        if ( tooHighLikelihood || !convergedFits  )
        {
            h_failed->Fill(t.scanpoint);
            if (t.scanpoint == 0) n0failed++;
            valid = false;
            nfailed++;
            //continue;
        }

        // Check if toys are in physical region.
        // Don't enforce t.chi2min-t.chi2minGlobal>0, else it can be hard because due
        // to little fluctuaions the best fit point can be missing from the plugin plot...
        bool inPhysicalRegion     = ((t.chi2minToy - t.chi2minGlobalToy) >= 0 );

        // build test statistic
        if ( valid && (t.chi2minToy - t.chi2minGlobalToy) >= (t.chi2min - this->chi2minGlobal) ) { //t.chi2minGlobal ){
            h_better->Fill(t.scanpoint);
        }
        if ( valid && (t.chi2minToy - t.chi2minGlobalToy) >= (t.chi2min - this->chi2minBkg) ) { //t.chi2minGlobal ){
            h_better_cls->Fill(t.scanpoint);
        }
        if (t.scanpoint == 0.0) n0better++;

        // goodness-of-fit
        if ( inPhysicalRegion && t.chi2minGlobalToy > this->chi2minGlobal ) { //t.chi2minGlobal ){
            h_gof->Fill(t.scanpoint);
        }
        // all toys
        if ( valid) { //inPhysicalRegion )
            // not efficient! TMath::Prob evaluated each toy, only needed once.
            // come up with smarter way
            h_all->Fill(t.scanpoint);
            h_probPValues->SetBinContent(h_probPValues->FindBin(t.scanpoint), this->getPValueTTestStatistic(t.chi2min - this->chi2minGlobal)); //t.chi2minGlobal));
            if (t.scanpoint == 0.0) n0all++;
        }
        int hBin = h_all->FindBin(t.scanpoint);
        if ( sampledBValues.find(hBin) == sampledBValues.end() ) sampledBValues[hBin] = std::vector<double>();
        if ( sampledSBValues.find(hBin) == sampledSBValues.end() ) sampledSBValues[hBin] = std::vector<double>();
        double bkgTestStatVal = t.chi2minBkgToy - t.chi2minGlobalBkgToy;
        bkgTestStatVal = t.scanbestBkg <= t.scanpoint ? bkgTestStatVal : 0.;  // if muhat < mu then q_mu = 0
        sampledBValues[hBin].push_back( bkgTestStatVal );
        double sbTestStatVal = t.chi2minToy - t.chi2minGlobalToy;
        sbTestStatVal = t.scanbest <= t.scanpoint ? sbTestStatVal : 0.; // if muhat < mu then q_mu = 0
        sampledSBValues[hBin].push_back( sbTestStatVal );

        // use the unphysical events to estimate background (be careful with this,
        // at least inspect the control plots to judge if this can be at all reasonable)
        if ( valid && !inPhysicalRegion ) {
            h_background->Fill(t.scanpoint);
        }

        if (n0tot % 1500 == 0 && n0all != 0) {
            //cout << "better: " << n0better << " all: " << n0all << " p: " << (float)n0better/(float)n0all << endl << endl;
            h_pVals->Fill((float)n0better / (float)n0all);
            n0tot = 0;
            n0better = 0;
            n0all = 0;
        }
    }
    cout << std::fixed << std::setprecision(2);
    cout << "MethodDatasetsPluginScan::readScan1dTrees() : reading done.           \n" << endl;
    cout << "MethodDatasetsPluginScan::readScan1dTrees() : read an average of " << ((double)nentries - (double)nfailed) / (double)nPoints1d << " toys per scan point." << endl;
    cout << "MethodDatasetsPluginScan::readScan1dTrees() : fraction of failed toys: " << (double)nfailed / (double)nentries * 100. << "%." << endl;
    cout << "MethodDatasetsPluginScan::readScan1dTrees() : fraction of background toys: " << h_background->GetEntries() / (double)nentries * 100. << "%." << endl;
    if ( nwrongrun > 0 ) {
        cout << "\nMethodDatasetsPluginScan::readScan1dTrees() : WARNING : Read toys that differ in global chi2min (wrong run) : "
             << (double)nwrongrun / (double)(nentries - nfailed) * 100. << "%.\n" << endl;
    }

    for (int i = 1; i <= h_better->GetNbinsX(); i++) {
        float nbetter = h_better->GetBinContent(i);
        float nbetter_cls = h_better_cls->GetBinContent(i);
        float nall = h_all->GetBinContent(i);
        // get number of background and failed toys
        float nbackground     = h_background->GetBinContent(i);

        nfailed       = h_failed->GetBinContent(i);

        //nall = nall - nfailed + nbackground;
        float ntot = h_tot->GetBinContent(i);
        if ( nall == 0. ) continue;
        h_background->SetBinContent(i, nbackground / nall);
        h_fracGoodToys->SetBinContent(i, (nall) / (float)ntot);
        // subtract background
        // float p = (nbetter-nbackground)/(nall-nbackground);
        // hCL->SetBinContent(i, p);
        // hCL->SetBinError(i, sqrt(p * (1.-p)/(nall-nbackground)));

        // don't subtract background
        float p = nbetter / nall;
        float p_cls = nbetter_cls / nall;
        hCL->SetBinContent(i, p);
        hCL->SetBinError(i, sqrt(p * (1. - p) / nall));
        hCLs->SetBinContent(i, p_cls);
        hCLs->SetBinError(i, sqrt(p_cls * (1. - p_cls) / nall));

        // the quantiles of the CLb distribution (for expected CLs)
        std::vector<double> probs  = { TMath::Prob(4,1), TMath::Prob(1,1), 0.5, 1.-TMath::Prob(1,1), 1.-TMath::Prob(4,1) };
        std::vector<double> clb_vals  = { 1.-TMath::Prob(4,1), 1.-TMath::Prob(1,1), 0.5, TMath::Prob(1,1), TMath::Prob(4,1) };
        std::vector<double> quantiles = Quantile<double>( sampledBValues[i], probs );
        std::vector<double> clsb_vals;
        //for (int k=0; k<quantiles.size(); k++) clsb_vals.push_back( TMath::Prob( quantiles[k], 1 ) );
        for (int k=0; k<quantiles.size(); k++ ){
          // asymptotic as chi2
          //clsb_vals.push_back( TMath::Prob( quantiles[k], 1 ) );
          // from toys
          clsb_vals.push_back( getVectorFracAboveValue( sampledSBValues[i], quantiles[k] ) );
        }

        // check
        if ( arg->debug ) {
          cout << i << endl;
          cout << "Quants: ";
          for (int k=0; k<quantiles.size(); k++) cout << quantiles[k] << " , ";
          cout << endl;
          cout << "CLb: ";
          for (int k=0; k<clb_vals.size(); k++) cout << clb_vals[k] << " , ";
          cout << endl;
          cout << "CLsb: ";
          for (int k=0; k<clsb_vals.size(); k++) cout << clsb_vals[k] << " , ";
          cout << endl;
          cout << "CLs: ";
          for (int k=0; k<clsb_vals.size(); k++) cout << clsb_vals[k]/clb_vals[k] << " , ";
          cout << endl;
        }

        hCLsExp->SetBinContent   ( i, TMath::Min( clsb_vals[2] / clb_vals[2] , 1.) );
        hCLsErr1Up->SetBinContent( i, TMath::Min( clsb_vals[1] / clb_vals[1] , 1.) );
        hCLsErr1Dn->SetBinContent( i, TMath::Min( clsb_vals[3] / clb_vals[3] , 1.) );
        hCLsErr2Up->SetBinContent( i, TMath::Min( clsb_vals[0] / clb_vals[0] , 1.) );
        hCLsErr2Dn->SetBinContent( i, TMath::Min( clsb_vals[4] / clb_vals[4] , 1.) );

        //hCLsExp->SetBinContent   ( i, (float(nSBValsAboveBkg[2]) / sampledSBValues[i].size() ) / probs[2] );
        //hCLsErr1Up->SetBinContent( i, (float(nSBValsAboveBkg[1]) / sampledSBValues[i].size() ) / probs[1] );
        //hCLsErr1Dn->SetBinContent( i, (float(nSBValsAboveBkg[3]) / sampledSBValues[i].size() ) / probs[3] );
        //hCLsErr2Up->SetBinContent( i, (float(nSBValsAboveBkg[0]) / sampledSBValues[i].size() ) / probs[0] );
        //hCLsErr2Dn->SetBinContent( i, (float(nSBValsAboveBkg[4]) / sampledSBValues[i].size() ) / probs[4] );

        // CLs values in data
        int nDataAboveBkgExp = 0;
        double dataTestStat = p>0 ? TMath::ChisquareQuantile(1.-p,1) : 1.e10;
        for (int j=0; j<sampledBValues[i].size(); j++ ) {
          if ( sampledBValues[i][j] >= dataTestStat ) nDataAboveBkgExp += 1;
        }
        float dataCLb    = float(nDataAboveBkgExp)/sampledBValues[i].size();
        float dataCLbErr = sqrt( dataCLb * (1.-dataCLb) / sampledBValues[i].size() );
        if ( p/dataCLb >= 1. ) {
          hCLsFreq->SetBinContent(i, 1.);
          hCLsFreq->SetBinError  (i, 0.);
        }
        else if ( dataTestStat == 1.e10 ) {
          hCLsFreq->SetBinContent(i, hCL->GetBinContent(i) );
          hCLsFreq->SetBinError  (i, hCL->GetBinError(i) );
        }
        else if ( hCLsFreq->GetBinCenter(i) <= hCLsFreq->GetBinCenter(h_better->GetMaximumBin()) ) {
          hCLsFreq->SetBinContent(i, 1.);
          hCLsFreq->SetBinError  (i, 0.);
        }
        else {
          hCLsFreq->SetBinContent(i, p / dataCLb);
          hCLsFreq->SetBinError  (i, (p/dataCLb) * sqrt( sq( hCL->GetBinError(i) / hCL->GetBinContent(i) ) + sq( dataCLbErr / dataCLb ) ) );
        }

        if (arg->debug) {
          cout << "At scanpoint " << std::scientific << hCL->GetBinCenter(i) << ": ===== number of toys for pValue calculation: " << nbetter << endl;
          cout << "At scanpoint " << hCL->GetBinCenter(i) << ": ===== pValue:         " << p << endl;
          cout << "At scanpoint " << hCL->GetBinCenter(i) << ": ===== pValue CLs:     " << p_cls << endl;
          cout << "At scanpoint " << hCL->GetBinCenter(i) << ": ===== pValue CLsFreq: " << hCLsFreq->GetBinContent(i) << endl;
        }
    }

    if ( arg->controlplot ) makeControlPlots( sampledBValues, sampledSBValues );

    if (arg->debug || drawPlots) {
        TCanvas* can = new TCanvas("can", "can", 1024, 786);
        can->cd();
        gStyle->SetOptTitle(0);
        //gStyle->SetOptStat(0);
        gStyle->SetPadTopMargin(0.05);
        gStyle->SetPadRightMargin(0.05);
        gStyle->SetPadBottomMargin(0.17);
        gStyle->SetPadLeftMargin(0.16);
        gStyle->SetLabelOffset(0.015, "X");
        gStyle->SetLabelOffset(0.015, "Y");
        h_fracGoodToys->SetXTitle(scanVar1);
        h_fracGoodToys->SetYTitle("fraction of good toys");
        h_fracGoodToys->Draw();
        TCanvas *canvas = new TCanvas("canvas", "canvas", 1200, 1000);
        canvas->Divide(2, 2);
        canvas->cd(1);
        h_all->SetXTitle("h_all");
        h_all->SetYTitle("number of toys");
        h_all->Draw();
        canvas->cd(2);
        h_better->SetXTitle("h_better");
        h_better->Draw();
        canvas->cd(3);
        h_gof->SetXTitle("h_gof");
        h_gof->Draw();
        canvas->cd(4);
        h_background->SetXTitle("h_bkg");
        h_background->SetYTitle("fraction of neg. test stat toys");
        h_background->Draw();
    }
    // goodness-of-fit

    int iBinBestFit = hCL->GetMaximumBin();
    float nGofBetter = h_gof->GetBinContent(iBinBestFit);
    float nall = h_all->GetBinContent(iBinBestFit);
    float fitprobabilityVal = nGofBetter / nall;
    float fitprobabilityErr = sqrt(fitprobabilityVal * (1. - fitprobabilityVal) / nall);
    cout << "MethodDatasetsPluginScan::readScan1dTrees() : fit prob of best-fit point: "
         << Form("(%.1f+/-%.1f)%%", fitprobabilityVal * 100., fitprobabilityErr * 100.) << endl;
}





double MethodDatasetsPluginScan::getPValueTTestStatistic(double test_statistic_value) {
    if ( test_statistic_value > 0) {
        // this is the normal case
        return TMath::Prob(test_statistic_value, 1);
    } else {
        if (arg->verbose) {
        cout << "MethodDatasetsPluginScan::scan1d_prob() : WARNING : Test statistic is negative, forcing it to zero" << std::endl
             << "Fit at current scan point has higher likelihood than free fit." << std::endl
             << "This should not happen except for very small underflows when the scan point is at the best fit value. " << std::endl
             << "Value of test statistic is " << test_statistic_value << std::endl
             << "An equal upwards fluctuaion corresponds to a p value of " << TMath::Prob(abs(test_statistic_value), 1) << std::endl;
        }
        // TMath::Prob will return 0 if the Argument is slightly below zero. As we are working with a float-zero we can not rely on it here:
        // TMath::Prob( 0 ) returns 1
        return 1.;
    }
}


///
/// Perform the 1d Plugin scan.
/// \param nRun Part of the root tree file name to facilitate parallel production.
///
int MethodDatasetsPluginScan::scan1d(int nRun)
{

    // //current working directory
    // boost::filesystem::path full_path( boost::filesystem::initial_path<boost::filesystem::path>() );
    // std::cout<<"initial path according to boost "<<full_path<<std::endl;

    // Necessary for parallelization
    RooRandom::randomGenerator()->SetSeed(0);
    // Set limit to all parameters.
    this->loadParameterLimits(); /// Default is "free", if not changed by cmd-line parameter


    // Define scan parameter and scan range.
    RooRealVar *parameterToScan = w->var(scanVar1);
    float parameterToScan_min = hCL->GetXaxis()->GetXmin();
    float parameterToScan_max = hCL->GetXaxis()->GetXmax();
    double freeDataFitValue = w->var(scanVar1)->getVal();

    TString probResName = Form("root/scan1dDatasetsProb_" + this->pdf->getName() + "_%ip" + "_" + scanVar1 + ".root", arg->npoints1d);
    TFile* probResFile = TFile::Open(probResName);
    if (!probResFile) {
        std::cout << "ERROR in MethodDatasetsPluginScan::scan1d - Prob scan result file not found in " << std::endl
                  << probResName << std::endl
                  << "Please run the prob scan before running the plugin scan. " << std::endl
                  << "The result file of the prob scan can be specified via the --probScanResult command line argument." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Define outputfile
    TString dirname = "root/scan1dDatasetsPlugin_" + this->pdf->getName() + "_" + scanVar1;
    system("mkdir -p " + dirname);
    TFile* outputFile = new TFile(Form(dirname + "/scan1dDatasetsPlugin_" + this->pdf->getName() + "_" + scanVar1 + "_run%i.root", nRun), "RECREATE");

    // Set up toy root tree
    ToyTree toyTree(this->pdf, arg);
    toyTree.init();
    toyTree.nrun = nRun;

    // Save parameter values that were active at function
    // call. We'll reset them at the end to be transparent
    // to the outside.
    RooDataSet* parsFunctionCall = new RooDataSet("parsFunctionCall", "parsFunctionCall", *w->set(pdf->getParName()));
    parsFunctionCall->add(*w->set(pdf->getParName()));

		// if CLs toys we need to keep hold of what's going on in the bkg only case
    // there is a small overhead here but it's necessary because the bkg only hypothesis
    // might not necessarily be in the scan range (although often it will be the first point)
		vector<RooDataSet*> cls_bkgOnlyToys;
    vector<float> chi2minGlobalBkgToysStore;
    vector<float> scanbestBkgToysStore;
    // Titus: Try importance sampling from the combination part -> works, but definitely needs improvement in precision
    int nActualToys = nToys;
    if ( arg->importance ){
        float plhPvalue = TMath::Prob(toyTree.chi2min - toyTree.chi2minGlobal,1);
        nActualToys = nToys*importance(plhPvalue);
    }
    for ( int j = 0; j < nActualToys; j++ ) {
      if(pdf->getBkgPdf()){
        pdf->generateBkgToys();
        pdf->generateToysGlobalObservables();
        RooDataSet* bkgOnlyToy = pdf->getBkgToyObservables();
        cls_bkgOnlyToys.push_back( (RooDataSet*)bkgOnlyToy->Clone() ); // clone required because of deleteToys() call at end of loop
        pdf->setToyData( bkgOnlyToy );
        parameterToScan->setConstant(false);
        RooFitResult *rb = loadAndFit(pdf);
        assert(rb);
        pdf->setMinNllScan(pdf->minNll);
        if (pdf->getFitStatus() != 0) {
            pdf->setFitStrategy(1);
            delete rb;
            rb = loadAndFit(pdf);
            pdf->setMinNllScan(pdf->minNll);
            assert(rb);

            if (pdf->getFitStatus() != 0) {
                pdf->setFitStrategy(2);
                delete rb;
                rb = loadAndFit(pdf);
                assert(rb);
            }
        }

        if (std::isinf(pdf->minNll) || std::isnan(pdf->minNll)) {
            cout  << "++++ > second and a half fit gives inf/nan: "  << endl
                  << "++++ > minNll: " << pdf->minNll << endl
                  << "++++ > status: " << pdf->getFitStatus() << endl;
            pdf->setFitStatus(-99);
        }
        pdf->setMinNllScan(pdf->minNll);

        chi2minGlobalBkgToysStore.push_back( 2 * rb->minNll() );
        scanbestBkgToysStore.push_back( ((RooRealVar*)w->set(pdf->getParName())->find(scanVar1))->getVal() );

        delete rb;
        pdf->deleteToys();
      }
    }

    // start scan
    cout << "MethodDatasetsPluginScan::scan1d_plugin() : starting ... with " << nPoints1d << " scanpoints..." << endl;
    ProgressBar progressBar(arg, nPoints1d);
    for ( int i = 0; i < nPoints1d; i++ )
    {

				toyTree.npoint = i;

				progressBar.progress();
        // scanpoint is calculated using min, max, which are the hCL x-Axis limits set in this->initScan()
        // this uses the "scan" range, as expected
        // don't add half the bin size. try to solve this within plotting method

        float scanpoint = parameterToScan_min + (parameterToScan_max - parameterToScan_min) * (double)i / ((double)nPoints1d - 1);
        toyTree.scanpoint = scanpoint;

				if ( i==0 && scanpoint != 0 ) {
					cout << "ERROR: For CLs option the first point in the scan must be zero not: " << scanpoint << endl;
					exit(1);
				}

        if (arg->debug) cout << "DEBUG in MethodDatasetsPluginScan::scan1d_plugin() - scanpoint in step " << i << " : " << scanpoint << endl;

        // don't scan in unphysical region
        // by default this means checking against "free" range
        if ( scanpoint < parameterToScan->getMin() || scanpoint > parameterToScan->getMax() + 2e-13 ) {
            cout << "not obvious: " << scanpoint << " < " << parameterToScan->getMin() << " and " << scanpoint << " > " << parameterToScan->getMax() + 2e-13 << endl;
            continue;
        }

        // Load the parameter values (nuisance parameters and parameter of interest) from the fit to data with fixed parameter of interest.
        this->setParevolPointByIndex(i);

        toyTree.statusScanData  = this->getParValAtIndex(i, "statusScanData");
        toyTree.chi2min         = this->getParValAtIndex(i, "chi2min");
        toyTree.covQualScanData = this->getParValAtIndex(i, "covQualScanData");

        // get the chi2 of the data
        if (this->chi2minGlobalFound) {
            toyTree.chi2minGlobal     = this->getChi2minGlobal();
        }
        else {
            cout << "FATAL in MethodDatasetsPluginScan::scan1d_plugin() - Global Minimum not set!" << endl;
            exit(EXIT_FAILURE);
        }

         toyTree.chi2minBkg     = this->getChi2minBkg();

        toyTree.storeParsPll();
        toyTree.genericProbPValue = this->getPValueTTestStatistic(toyTree.chi2min - toyTree.chi2minGlobal);

        // Titus: Try importance sampling from the combination part -> works, but definitely needs improvement in precision
        int nActualToys = nToys;
        if ( arg->importance ){
            float plhPvalue = TMath::Prob(toyTree.chi2min - toyTree.chi2minGlobal,1);
            nActualToys = nToys*importance(plhPvalue);
        }
        //Titus: Debug histogram to see the different deltachisq distributions
        TH1F histdeltachi2("histdeltachi2", "histdeltachi2", 200,0,5);
        for ( int j = 0; j < nActualToys; j++ )
        {
            if (arg->debug) cout << ">> new toy\n" << endl;
            this->pdf->setMinNllFree(0);
            this->pdf->setMinNllScan(0);

						toyTree.ntoy = j;

            // 1. Generate toys

            // For toy generation, set all parameters (nuisance parameters and parameter of interest) to the values from the constrained
            // fit to data with fixed parameter of interest.
            // This is called the PLUGIN method.
            this->setParevolPointByIndex(i);

            this->pdf->generateToys(); // this is generating the toy dataset
            this->pdf->generateToysGlobalObservables(); // this is generating the toy global observables and saves globalObs in snapshot


            // TODO - DELETE THE FOLLOWING SMALL BLOCK
            // keep hold of the background only toys -- this part is broken somehow
            //if(i==0){
              //if(pdf->getBkgPdf()){
                //pdf->generateBkgToys();
                //cls_bkgOnlyToys.push_back( (RooDataSet*)this->pdf->getBkgToyObservables()->Clone() ); //surely not optimal
              //}
              //else {
              //cls_bkgOnlyToys.push_back( (RooDataSet*)this->pdf->getToyObservables()->Clone() ); // clone required because of deleteToys() call at end of loop
                //}
            //}
            //else {
              //assert( cls_bkgOnlyToys.size() == nToys );
            //}

            // \todo: comment the following back in once I know what it does ...
            //      t.storeParsGau( we need to pass a rooargset of the means of the global observables here);

            //
            // 2. Fit to toys with parameter of interest fixed to scanpoint
            //
            if (arg->debug)cout << "DEBUG in MethodDatasetsPluginScan::scan1d_plugin() - perform scan toy fit" << endl;

            // set parameters to constrained data scan fit result again
            this->setParevolPointByIndex(i);

            // fixed parameter of interest
            parameterToScan->setConstant(true);
            this->pdf->setFitStrategy(0);
            RooFitResult* r   = this->loadAndFit(this->pdf);
            assert(r);
            pdf->setMinNllScan(pdf->minNll);

            this->setAndPrintFitStatusFreeToys(toyTree);

            if (pdf->getFitStatus() != 0) {
                pdf->setFitStrategy(1);
                delete r;
                r = this->loadAndFit(this->pdf);
                pdf->setMinNllScan(pdf->minNll);
                assert(r);

                this->setAndPrintFitStatusFreeToys(toyTree);

                if (pdf->getFitStatus() != 0) {
                    pdf->setFitStrategy(2);
                    delete r;
                    r = this->loadAndFit(this->pdf);
                    assert(r);
                }
            }

            if (std::isinf(pdf->minNll) || std::isnan(pdf->minNll)) {
                cout  << "++++ > second fit gives inf/nan: "  << endl
                      << "++++ > minNll: " << pdf->minNll << endl
                      << "++++ > status: " << pdf->getFitStatus() << endl;
                pdf->setFitStatus(-99);
            }
            pdf->setMinNllScan(pdf->minNll);


            toyTree.chi2minToy          = 2 * r->minNll(); // 2*r->minNll(); //2*r->minNll();
            toyTree.chi2minToyPDF       = 2 * pdf->getMinNllScan();
            toyTree.covQualScan         = r->covQual();
            toyTree.statusScan          = r->status();
            toyTree.statusScanPDF       = pdf->getFitStatus(); //r->status();
            toyTree.storeParsScan();

            pdf->deleteNLL();

            RooDataSet* parsAfterScanFit = new RooDataSet("parsAfterScanFit", "parsAfterScanFit", *w->set(pdf->getParName()));
            parsAfterScanFit->add(*w->set(pdf->getParName()));

            //
            // 2.5 Fit to bkg only toys with parameter of interest fixed to scanpoint
            //
            if (arg->debug)cout << "DEBUG in MethodDatasetsPluginScan::scan1d_plugin() - perform scan toy fit to background" << endl;

            // set parameters to constrained data scan fit result again
            this->setParevolPointByIndex(i);

            // fixed parameter of interest
            parameterToScan->setConstant(true);
            this->pdf->setFitStrategy(0);
            // temporarily store our current toy here so we can put it back in a minute
            RooDataSet *tempData = (RooDataSet*)this->pdf->getToyObservables();
            // now get our background only toy (to fit under this hypothesis)
            RooDataSet *bkgToy = (RooDataSet*)cls_bkgOnlyToys[j];
            if (arg->debug) cout << "Setting background toy as data " << bkgToy << endl;
            this->pdf->setToyData( bkgToy );
            RooFitResult* rb   = this->loadAndFit(this->pdf);
            assert(rb);
            pdf->setMinNllScan(pdf->minNll);

            this->setAndPrintFitStatusFreeToys(toyTree);

            if (pdf->getFitStatus() != 0) {
                pdf->setFitStrategy(1);
                delete rb;
                rb = this->loadAndFit(this->pdf);
                pdf->setMinNllScan(pdf->minNll);
                assert(rb);

                this->setAndPrintFitStatusFreeToys(toyTree);

                if (pdf->getFitStatus() != 0) {
                    pdf->setFitStrategy(2);
                    delete rb;
                    rb = this->loadAndFit(this->pdf);
                    assert(rb);
                }
            }

            if (std::isinf(pdf->minNll) || std::isnan(pdf->minNll)) {
                cout  << "++++ > second and a half fit gives inf/nan: "  << endl
                      << "++++ > minNll: " << pdf->minNll << endl
                      << "++++ > status: " << pdf->getFitStatus() << endl;
                pdf->setFitStatus(-99);
            }
            pdf->setMinNllScan(pdf->minNll);


            toyTree.chi2minBkgToy          = 2 * rb->minNll(); // 2*r->minNll(); //2*r->minNll();
            toyTree.chi2minBkgToyPDF       = 2 * pdf->getMinNllScan();

            pdf->deleteNLL();

            //
            // 3. Fit to toys with free parameter of interest
            //
            if (arg->debug)cout << "DEBUG in MethodDatasetsPluginScan::scan1d_plugin() - perform free toy fit" << endl;
            // Use parameters from the scanfit to data

            this->setParevolPointByIndex(i);

            // free parameter of interest
            parameterToScan->setConstant(false);
            //setLimit(w, scanVar1, "free");
            w->var(scanVar1)->removeRange();

						// set dataset back
						if (arg->debug) cout << "Setting toy back as data " << tempData << endl;
						this->pdf->setToyData( tempData );

            // Fit
            pdf->setFitStrategy(0);
            RooFitResult* r1  = this->loadAndFit(this->pdf);
            assert(r1);
            pdf->setMinNllFree(pdf->minNll);
            toyTree.chi2minGlobalToy = 2 * r1->minNll();

            if (! std::isfinite(pdf->getMinNllFree())) {
                cout << "----> nan/inf flag detected " << endl;
                cout << "----> fit status: " << pdf->getFitStatus() << endl;
                pdf->setFitStatus(-99);
            }

            bool negTestStat = toyTree.chi2minToy - toyTree.chi2minGlobalToy < 0;

            this->setAndPrintFitStatusConstrainedToys(toyTree);


            if (pdf->getFitStatus() != 0 || negTestStat ) {

                pdf->setFitStrategy(1);

                if (arg->verbose) cout << "----> refit with strategy: 1" << endl;
                delete r1;
                r1  = this->loadAndFit(this->pdf);
                assert(r1);
                pdf->setMinNllFree(pdf->minNll);
                toyTree.chi2minGlobalToy = 2 * r1->minNll();
                if (! std::isfinite(pdf->getMinNllFree())) {
                    cout << "----> nan/inf flag detected " << endl;
                    cout << "----> fit status: " << pdf->getFitStatus() << endl;
                    pdf->setFitStatus(-99);
                }
                negTestStat = toyTree.chi2minToy - toyTree.chi2minGlobalToy < 0;

                this->setAndPrintFitStatusConstrainedToys(toyTree);

                if (pdf->getFitStatus() != 0 || negTestStat ) {

                    pdf->setFitStrategy(2);

                    if (arg->verbose) cout << "----> refit with strategy: 2" << endl;
                    delete r1;
                    r1  = this->loadAndFit(this->pdf);
                    assert(r1);
                    pdf->setMinNllFree(pdf->minNll);
                    toyTree.chi2minGlobalToy = 2 * r1->minNll();
                    if (! std::isfinite(pdf->getMinNllFree())) {
                        cout << "----> nan/inf flag detected " << endl;
                        cout << "----> fit status: " << pdf->getFitStatus() << endl;
                        pdf->setFitStatus(-99);
                    }
                    this->setAndPrintFitStatusConstrainedToys(toyTree);

                    if ( (toyTree.chi2minToy - toyTree.chi2minGlobalToy) < 0) {
                        cout << "+++++ > still negative test statistic after whole procedure!! " << endl;
                        cout << "+++++ > try to fit with different starting values" << endl;
                        cout << "+++++ > dChi2: " << toyTree.chi2minToy - toyTree.chi2minGlobalToy << endl;
                        cout << "+++++ > dChi2PDF: " << 2 * (pdf->getMinNllScan() - pdf->getMinNllFree()) << endl;
                        Utils::setParameters(this->pdf->getWorkspace(), pdf->getParName(), parsAfterScanFit->get(0));
                        if (parameterToScan->getVal() < 1e-13) parameterToScan->setVal(0.67e-12);
                        parameterToScan->setConstant(false);
                        pdf->deleteNLL();
                        RooFitResult* r_tmp = this->loadAndFit(this->pdf);
                        assert(r_tmp);
                        if (r_tmp->status() == 0 && r_tmp->minNll() < r1->minNll() && r_tmp->minNll() > -1e27) {
                            pdf->setMinNllFree(pdf->minNll);
                            cout << "+++++ > Improvement found in extra fit: Nll before: " << r1->minNll()
                                 << " after: " << r_tmp->minNll() << endl;
                            delete r1;
                            r1 = r_tmp;
                            cout << "+++++ > new minNll value: " << r1->minNll() << endl;
                        }
                        else {
                            // set back parameter value to last fit value
                            cout << "+++++ > no Improvement found, reset ws par value to last fit result" << endl;
                            parameterToScan->setVal(static_cast<RooRealVar*>(r1->floatParsFinal().find(parameterToScan->GetName()))->getVal());
                            delete r_tmp;
                        }
                        delete parsAfterScanFit;
                    };
                    if (arg->debug) {
                        cout  << "===== > compare free fit result with pdf parameters: " << endl;
                        cout  << "===== > minNLL for fitResult: " << r1->minNll() << endl
                              << "===== > minNLL for pdfResult: " << pdf->getMinNllFree() << endl
                              << "===== > status for pdfResult: " << pdf->getFitStatus() << endl
                              << "===== > status for fitResult: " << r1->status() << endl;
                    }
                }
            }
            // set the limit back again
            setLimit(w, scanVar1, "scan");

            toyTree.chi2minGlobalToy    = 2 * r1->minNll(); //2*r1->minNll();
            toyTree.chi2minGlobalToyPDF = 2 * pdf->getMinNllFree(); //2*r1->minNll();
            toyTree.statusFreePDF       = pdf->getFitStatus(); //r1->status();
            toyTree.statusFree          = r1->status();
            toyTree.covQualFree         = r1->covQual();
            toyTree.scanbest            = ((RooRealVar*)w->set(pdf->getParName())->find(scanVar1))->getVal();
            toyTree.storeParsFree();
            pdf->deleteNLL();

            // now save the global min (and best fit val) for the background only toy for later (this just makes life easier even though we are duplicating a number in the tree)
            //if ( i==0 ) {
              //chi2minGlobalBkgToysStore.push_back( toyTree.chi2minGlobalToy );
              //scanbestBkgToysStore.push_back( toyTree.scanbest );
            //}
            //else {
              assert( chi2minGlobalBkgToysStore.size() == nToys );
              assert( scanbestBkgToysStore.size() == nToys );
              //}
            toyTree.chi2minGlobalBkgToy = chi2minGlobalBkgToysStore[j];
            toyTree.scanbestBkg      = scanbestBkgToysStore[j];

            if (arg->debug) {
                cout << "#### > Fit summary: " << endl;
                cout  << "#### > free fit status: " << toyTree.statusFree << " vs pdf: " << toyTree.statusFreePDF << endl
                      << "#### > scan fit status: " << toyTree.statusScan << " vs pdf: " << toyTree.statusScanPDF << endl
                      << "#### > free min nll: " << toyTree.chi2minGlobalToy << " vs pdf: " << toyTree.chi2minGlobalToyPDF << endl
                      << "#### > scan min nll: " << toyTree.chi2minToy << " vs pdf: " << toyTree.chi2minToyPDF << endl
                      << "#### > dChi2 fitresult: " << toyTree.chi2minToy - toyTree.chi2minGlobalToy << endl
                      << "#### > dChi2 pdfresult: " << toyTree.chi2minToyPDF - toyTree.chi2minGlobalToyPDF << endl;
                cout  << std::setprecision(6);

                if (toyTree.chi2minToy - toyTree.chi2minGlobalToy > 20 && (toyTree.statusFree == 0 && toyTree.statusScan == 0)
                        && toyTree.chi2minToy > -1e27 && toyTree.chi2minGlobalToy > -1e27) {
                    cout << std::setw(30) << std::setfill('-') << ">>> HIGH test stat value!! print fit results with fit strategy: " << pdf->getFitStrategy() << std::setfill(' ') << endl;
                    cout << "SCAN FIT Result" << endl;
                    r->Print("");
                    cout << "================" << endl;
                    cout << "FREE FIT result" << endl;
                    r1->Print("");
                }

                cout << "DEBUG in MethodDatasetsPluginScan::scan1d_plugin() - ToyTree 2*minNll free fit: " << toyTree.chi2minGlobalToy << endl;
            }

            //
            // 4. store
            //

            //Titus:Debug fill deltachisq hist
            if (arg->debug) histdeltachi2.Fill(toyTree.chi2minToy-toyTree.chi2minGlobalToy);

            toyTree.fill();
            //remove dataset and pointers
            delete r;
            delete r1;
            delete rb;
            pdf->deleteToys();
        } // End of toys loop

        // reset
        setParameters(w, pdf->getParName(), parsFunctionCall->get(0));
        //delete result;

        //setParameters(w, pdf->getObsName(), obsDataset->get(0));

        //Titus Draw debug histdeltachisq
        if (arg->debug)
        {
            TCanvas histplot("histplot", "Delta chi2 toys", 1024, 786);
            histdeltachi2.Draw();
            string plotstring = "plots/pdf/deltachi2_"+to_string(i)+".pdf";
            histplot.SaveAs(plotstring.c_str());
        }

    } // End of npoints loop
    toyTree.writeToFile();
    outputFile->Close();
    delete parsFunctionCall;
    return 0;
}



void MethodDatasetsPluginScan::drawDebugPlots(int runMin, int runMax, TString fileNameBaseIn) {
    int nFilesRead, nFilesMissing;
    TChain* c = this->readFiles(runMin, runMax, nFilesRead, nFilesMissing, fileNameBaseIn);
    //ToyTree t(this->pdf, c);
    //t.open();
    cout << "does it take long?" << endl;

    TString cut = "scanpoint == 0 && statusScan == 0 && statusFree == 0 && abs(chi2minToy)<300e3 && abs(chi2minGlobalToy)<300e3";
    TString isphysical = "(chi2minToy-chi2minGlobalToy)>=0";
    TCanvas* can = new TCanvas("can", "DChi2Nominal", 1024, 786);
    TCanvas* can1 = new TCanvas("can1", "BR_{Bd}", 1024, 786);
    TCanvas* can3 = new TCanvas("can3", "Chi2distr", 1024, 786);

    TCanvas* can2 = new TCanvas("can2", "DChi2False", 1024, 786);
    can->cd();
    chain->Draw("chi2minToy-chi2minGlobalToy", cut + "&&" + isphysical + " && abs(chi2minToy-chi2minGlobalToy)<1e2", "norm");
    can1->cd();
    chain->Draw("BR_{Bd}_free", cut + "&&" + isphysical, "norm");
    can2->cd();
    chain->Draw("chi2minToy-chi2minGlobalToy", "!(" + cut + "&&" + isphysical + ") && abs(chi2minToy-chi2minGlobalToy)<1e2", "norm");
    can3->cd();
    c->Draw("chi2minToy", cut, "norm");
    c->Draw("chi2minGlobalToy", cut, "normSAME");
    //cout << "draw takes a while" << endl;
};


///
/// Assumption: root file is given to the scanner which only has toy at a specific scanpoint, not necessary!
///
void MethodDatasetsPluginScan::performBootstrapTest(int nSamples, const TString& ext) {
    TRandom3 rndm;
    TH1F* hist = new TH1F("h", "h", 800, 1e-4, 0.008);
    bootstrapPVals.clear();
    int nFilesRead(0), nFilesMissing(0);
    this->readFiles(arg->jmin[0], arg->jmax[0],
                    nFilesRead, nFilesMissing, arg->jobdir);
    ToyTree t(this->pdf, this->arg, this->chain);
    t.open();
    t.activateCoreBranchesOnly(); ///< speeds up the event loop

    TString cut = "";
    // Define cuts
    cut += "scanpoint == 0";
    cut += " && statusScan == 0";
    cut += " && statusFree == 0";
    cut += " && abs(chi2minToy)<1e27";
    cut += " && abs(chi2minGlobalToy)<1e27";
    //cut += " && (chi2minToy-2*chi2minGlobalToy)>=0";

    double numberOfToys  = chain->GetEntries(cut);

    //numberOfToys = 500;

    std::vector<int> failed;
    std::vector<double> q;
    std::vector<double> q_Status_gt0;
    failed.clear();
    int totFailed = 0;
    // define bootstrap sample
    double q_data = 0;
    for (int i = 0; i < t.GetEntries(); i++) {
        t.GetEntry(i);
        if (i == 0) {
            q_data = t.chi2min - t.chi2minGlobal;
            cout << "Test stat for data: " << q_data << endl;
        }
        if (!(t.statusScan == 0 && t.statusFree == 0 && fabs(t.chi2minToy) < 1e27
                && fabs(t.chi2minGlobalToy) < 1e27 && t.scanpoint == 0))
        {
            totFailed++;
            /*
            if( (t.statusFree == 0 && t.statusScan ==1)
                    || (t.statusFree == 1 && t.statusScan ==0)
                    || (t.statusFree == 1 && t.statusScan ==1)){
                cout  << "Check test stat for status>0: dChi2 = "
                            << t.chi2minToy-t.chi2minGlobalToy << endl;
                q_Status_gt0.push_back(t.chi2minToy-t.chi2minGlobalToy);
            }
            */
            failed.push_back(i);
            continue;
        }

        q.push_back(t.chi2minToy - t.chi2minGlobalToy);

    }
    cout  << "INFO in MethodDatasetsPluginScan::performBootstrapTest - Tree loop finished" << endl;
    cout  << "- start BootstrapTest with " << nSamples
          << " Samples and " << numberOfToys << " Toys each" << endl;
    cout  << " Total number failed: " << totFailed << endl;

    for (int i = 0; i < nSamples; ++i) {
        int nSelected   = 0;
        double nbetter  = 0;
        for (int j = 0; j < numberOfToys; j++) {

            int rndmInt = -1;
            do {
                rndmInt = rndm.Integer(numberOfToys);
            } while (std::find(failed.begin(), failed.end(), rndmInt) != failed.end());

            if ( (q[rndmInt]) > q_data ) nbetter += 1;
        }
        double p = nbetter / numberOfToys;
        bootstrapPVals.push_back(p);
        hist->Fill(p);
        if (i % 100 == 0) cout << i << " Samples from " << nSamples << " done. p Value: " << p << " with " << nbetter << " Toys of " << numberOfToys << " total" << endl;
    }
    TCanvas* c = new TCanvas("c", "c", 1024, 768);
    hist->SetLineColor(kRed + 2);
    hist->SetLineWidth(2);
    hist->Fit("gaus");
    hist->Draw();

    c->SaveAs(Form("plots/root/" + name + "_bootStrap_%i_samples_with_%i_toys_" + ext + ".root", nSamples, numberOfToys));
    c->SaveAs(Form("plots/C/" + name + "_bootStrap_%i_samples_with_%i_toys_" + ext + ".C", nSamples, numberOfToys));
    c->SaveAs(Form("plots/pdf/" + name + "_bootStrap_%i_samples_with_%i_toys_" + ext + ".pdf", nSamples, numberOfToys));
    c->SaveAs(Form("plots/png/" + name + "_bootStrap_%i_samples_with_%i_toys_" + ext + ".png", nSamples, numberOfToys));

    return;

};

void MethodDatasetsPluginScan::printDebug(const RooFitResult& r) {
    cout << std::fixed << std::scientific;
    cout << std::setw(42) << std::right << std::setfill('-') << " Minimum: "  << std::setprecision(8) << r.minNll()
         << " with edm: " << std::setprecision(6) << r.edm() << endl;
    cout  << std::setw(42) << std::right << std::setfill('-')
          << " Minimize status: " << r.status() << endl;
    cout  << std::setw(42) << std::right << std::setfill('-')
          << " Number of invalid NLL evaluations: " << r.numInvalidNLL() << endl;
    cout  << std::resetiosflags(std::ios::right)
          << std::resetiosflags(std::ios::fixed)
          << std::resetiosflags(std::ios::scientific);
};


RooSlimFitResult* MethodDatasetsPluginScan::getParevolPoint(float scanpoint) {
    std::cout << "ERROR: not implemented for MethodDatasetsPluginScan, use setParevolPointByIndex() instad" << std::endl;
    exit(EXIT_FAILURE);
}


///
/// Load the param. values from the data-fit at a certain scan point
///
void MethodDatasetsPluginScan::setParevolPointByIndex(int index) {



    this->getProfileLH()->probScanTree->t->GetEntry(index);
    RooArgSet* pars          = (RooArgSet*)this->pdf->getWorkspace()->set(pdf->getParName());

    //\todo: make sure this is checked during pdf init, do not check again here
    if (!pars) {
        cout << "MethodDatasetsPluginScan::setParevolPointByIndex(int index) : ERROR : no parameter set found in workspace!" << endl;
        exit(EXIT_FAILURE);
    }

    TIterator* it = pars->createIterator();
    while ( RooRealVar* p = (RooRealVar*)it->Next() ) {
        TString parName     = p->GetName();
        TLeaf* parLeaf      = (TLeaf*) this->getProfileLH()->probScanTree->t->GetLeaf(parName + "_scan");
        if (!parLeaf) {
            cout << "MethodDatasetsPluginScan::setParevolPointByIndex(int index) : ERROR : no var (" << parName
                 << ") found in PLH scan file!" << endl;
            exit(EXIT_FAILURE);
        }
        float scanParVal    = parLeaf->GetValue();
        p->setVal(scanParVal);
    }
}



void MethodDatasetsPluginScan::setAndPrintFitStatusConstrainedToys(const ToyTree& toyTree) {

    if (pdf->getMinNllScan() != 0 && (pdf->getMinNllFree() > pdf->getMinNllScan())) {
        // create unique failureflag
        switch (pdf->getFitStatus())
        {
        case 0:
            pdf->setFitStatus(-13);
            break;
        case 1:
            pdf->setFitStatus(-12);
            break;
        case -1:
            pdf->setFitStatus(-33);
            break;
        case -99:
            pdf->setFitStatus(-66);
            break;
        default:
            pdf->setFitStatus(-100);
            break;

        }
    }

    bool negTestStat = toyTree.chi2minToy - toyTree.chi2minGlobalToy < 0;

    if ( (pdf->getFitStatus() != 0 || negTestStat ) && arg->debug ) {
        cout  << "----> problem in current fit: going to refit with strategy " << pdf->getFitStrategy() << " , summary: " << endl
              << "----> NLL value: " << std::setprecision(9) << pdf->getMinNllFree() << endl
              << "----> fit status: " << pdf->getFitStatus() << endl
              << "----> dChi2: " << (toyTree.chi2minToy - toyTree.chi2minGlobalToy) << endl
              << "----> dChi2PDF: " << 2 * (pdf->getMinNllScan() - pdf->getMinNllFree()) << endl;

        switch (pdf->getFitStatus()) {
        case 1:
            cout << "----> fit results in status 1" << endl;
            cout << "----> NLL value: " << pdf->getMinNllFree() << endl;
            break;

        case -1:
            cout << "----> fit results in status -1" << endl;
            cout << "----> NLL value: " << pdf->getMinNllFree() << endl;
            break;

        case -99:
            cout << "----> fit has NLL value with flag NaN or INF" << endl;
            cout << "----> NLL value: " << pdf->getMinNllFree() << endl;
            break;
        case -66:
            cout  << "----> fit has nan/inf NLL value and a negative test statistic" << endl
                  << "----> dChi2: " << 2 * (pdf->getMinNllScan() - pdf->getMinNllFree()) << endl
                  << "----> scan fit min nll:" << pdf->getMinNllScan() << endl
                  << "----> free fit min nll:" << pdf->getMinNllFree() << endl;
            break;
        case -13:
            cout  << "----> free fit has status 0 but creates a negative test statistic" << endl
                  << "----> dChi2: " << 2 * (pdf->getMinNllScan() - pdf->getMinNllFree()) << endl
                  << "----> scan fit min nll:" << pdf->getMinNllScan() << endl
                  << "----> free fit min nll:" << pdf->getMinNllFree() << endl;
            break;
        case -12:
            cout  << "----> free fit has status 1 and creates a negative test statistic" << endl
                  << "----> dChi2: " << 2 * (pdf->getMinNllScan() - pdf->getMinNllFree()) << endl
                  << "----> scan fit min nll:" << pdf->getMinNllScan() << endl
                  << "----> free fit min nll:" << pdf->getMinNllFree() << endl;

            break;
        case -33:
            cout  << "----> free fit has status -1 and creates a negative test statistic" << endl
                  << "----> dChi2: " << 2 * (pdf->getMinNllScan() - pdf->getMinNllFree()) << endl
                  << "----> scan fit min nll:" << pdf->getMinNllScan() << endl
                  << "----> free fit min nll:" << pdf->getMinNllFree() << endl;
            cout  << std::setprecision(6);
            break;
        default:
            cout << "-----> unknown / fitResult neg test stat, but status" << pdf->getFitStatus() << endl;
            break;
        }
    }
}


void MethodDatasetsPluginScan::setAndPrintFitStatusFreeToys(const ToyTree& toyTree) {

    if (! std::isfinite(pdf->getMinNllScan())) {
        if ( arg->debug) {
          cout << "----> nan/inf flag detected " << endl;
          cout << "----> fit status: " << pdf->getFitStatus() << endl;
        }
        pdf->setFitStatus(-99);
    }

    if (pdf->getFitStatus() != 0 && arg->debug) {
        cout  << "----> problem in current fit: going to refit with strategy 1, summary: " << endl
              << "----> NLL value: " << std::setprecision(9) << pdf->minNll << endl
              << "----> fit status: " << pdf->getFitStatus() << endl;
        switch (pdf->getFitStatus()) {
        case 1:
            cout << "----> fit results in status 1" << endl;
            cout << "----> NLL value: " << pdf->minNll << endl;
            // cout << "----> edm: " << r->edm() << endl;
            break;

        case -1:
            cout << "----> fit results in status -1" << endl;
            cout << "----> NLL value: " << pdf->minNll << endl;
            // cout << "----> edm: " << r->edm() << endl;
            break;

        case -99:
            cout << "----> fit has NLL value with flag NaN or INF" << endl;
            cout << "----> NLL value: " << pdf->minNll << endl;
            // cout << "----> edm: " << r->edm() << endl;
            break;

        default:
            cout << "unknown" << endl;
            break;
        }
    }
}

void MethodDatasetsPluginScan::makeControlPlots(map<int, vector<double> > bVals, map<int, vector<double> > sbVals)
{
  // the quantiles of the CLb distribution (for expected CLs)
  std::vector<double> probs  = { TMath::Prob(4,1), TMath::Prob(1,1), 0.5, 1.-TMath::Prob(1,1), 1.-TMath::Prob(4,1) };
  std::vector<double> clb_vals  = { 1.-TMath::Prob(4,1), 1.-TMath::Prob(1,1), 0.5, TMath::Prob(1,1), TMath::Prob(4,1) };

  for ( int i=1; i<= hCLs->GetNbinsX(); i++ ) {

    std::vector<double> quantiles = Quantile<double>( bVals[i], probs );
    std::vector<double> clsb_vals;
    for (int k=0; k<quantiles.size(); k++ ){
      clsb_vals.push_back( getVectorFracAboveValue( sbVals[i], quantiles[k] ) );
    }
    TCanvas *c = newNoWarnTCanvas( Form("q%d",i), Form("q%d",i) );
    double max = *(std::max_element( bVals[i].begin(), bVals[i].end() ) );
    TH1F *hb = new TH1F( Form("hb%d",i), "hbq", 50,0, max );
    TH1F *hsb = new TH1F( Form("hsb%d",i), "hsbq", 50,0, max );

    for ( int j=0; j<bVals[i].size(); j++ ) hb->Fill( bVals[i][j] );
    for ( int j=0; j<sbVals[i].size(); j++ ) hsb->Fill( sbVals[i][j] );

    double dataVal = TMath::ChisquareQuantile( 1.-hCL->GetBinContent(i),1 );
    TArrow *lD = new TArrow( dataVal, 0.6*hsb->GetMaximum(), dataVal, 0., 0.15, "|>" );

    vector<TLine*> qLs;
    for ( int k=0; k<quantiles.size(); k++ ) {
      qLs.push_back( new TLine( quantiles[k], 0, quantiles[k], 0.8*hsb->GetMaximum() ) );
    }
    TLatex *lat = new TLatex();
    lat->SetTextColor(kRed);
    lat->SetTextSize(0.6*lat->GetTextSize());
    lat->SetTextAlign(22);

    hsb->GetXaxis()->SetTitle("Test Statistic Value");
    hsb->GetYaxis()->SetTitle("Entries");
    hsb->GetXaxis()->SetTitleSize(0.06);
    hsb->GetYaxis()->SetTitleSize(0.06);
    hsb->GetXaxis()->SetLabelSize(0.06);
    hsb->GetYaxis()->SetLabelSize(0.06);
    hsb->SetLineWidth(2);
    hb->SetLineWidth(2);
    hsb->SetFillColor(kBlue);
    hb->SetFillColor(kRed);
    hsb->SetFillStyle(3003);
    hb->SetFillStyle(3004);
    hb->SetLineColor(kRed);
    hsb->SetLineColor(kBlue);

    //TGraph *gb = Utils::smoothHist(hb, 0);
    //TGraph *gsb = Utils::smoothHist(hsb, 1);

    //gb->SetLineColor(kRed+1);
    //gb->SetLineWidth(4);
    //gsb->SetLineColor(kBlue+1);
    //gsb->SetLineWidth(4);

    hsb->Draw();
    hb->Draw("same");
    //gb->Draw("Lsame");
    //gsb->Draw("Lsame");

    qLs[0]->SetLineWidth(2);
    qLs[0]->SetLineStyle(kDashed);
    qLs[4]->SetLineWidth(2);
    qLs[4]->SetLineStyle(kDashed);
    qLs[1]->SetLineWidth(3);
    qLs[3]->SetLineWidth(3);
    qLs[2]->SetLineWidth(5);

    for ( int k=0; k<quantiles.size(); k++ ){
      qLs[k]->SetLineColor(kRed);
      qLs[k]->Draw("same");
    }
    lat->DrawLatex( quantiles[0], hsb->GetMaximum(), "-2#sigma" );
    lat->DrawLatex( quantiles[1], hsb->GetMaximum(), "-1#sigma" );
    lat->DrawLatex( quantiles[2], hsb->GetMaximum(), "<B>" );
    lat->DrawLatex( quantiles[3], hsb->GetMaximum(), "+1#sigma" );
    lat->DrawLatex( quantiles[4], hsb->GetMaximum(), "+2#sigma" );

    lD->SetLineColor(kBlack);
    lD->SetLineWidth(5);
    lD->Draw("same");

    TLegend *leg = new TLegend(0.74,0.54,0.94,0.7);
    leg->SetHeader(Form("p=%4.2g",hCLs->GetBinCenter(i)));
    leg->SetFillColor(0);
    leg->AddEntry(hb,"B-only Toys","LF");
    leg->AddEntry(hsb,"S+B Toys","LF");
    leg->AddEntry(lD,"Data","L");
    leg->Draw("same");
    c->SetLogy();
    savePlot(c,Form("cls_testStatControlPlot_p%d",i) );
  }

  TCanvas *c = newNoWarnTCanvas( "cls_ctr", "CLs Control" );
  hCLsFreq->SetLineColor(kBlack);
  hCLsFreq->SetLineWidth(3);
  hCLsExp->SetLineColor(kRed);
  hCLsExp->SetLineWidth(3);

  hCLsErr1Up->SetLineColor(kBlue+2);
  hCLsErr1Up->SetLineWidth(2);
  hCLsErr1Dn->SetLineColor(kBlue+2);
  hCLsErr1Dn->SetLineWidth(2);

  hCLsErr2Up->SetLineColor(kBlue+2);
  hCLsErr2Up->SetLineWidth(2);
  hCLsErr2Up->SetLineStyle(kDashed);
  hCLsErr2Dn->SetLineColor(kBlue+2);
  hCLsErr2Dn->SetLineWidth(2);
  hCLsErr2Dn->SetLineStyle(kDashed);

  hCLsFreq->GetXaxis()->SetTitle("POI");
  hCLsFreq->GetYaxis()->SetTitle("Raw CLs");
  hCLsFreq->GetXaxis()->SetTitleSize(0.06);
  hCLsFreq->GetYaxis()->SetTitleSize(0.06);
  hCLsFreq->GetXaxis()->SetLabelSize(0.06);
  hCLsFreq->GetYaxis()->SetLabelSize(0.06);

  hCLsFreq->Draw("L");
  hCLsErr2Up->Draw("Lsame");
  hCLsErr2Dn->Draw("Lsame");
  hCLsErr1Up->Draw("Lsame");
  hCLsErr1Dn->Draw("Lsame");
  hCLsExp->Draw("Lsame");
  hCLsFreq->Draw("Lsame");

  savePlot(c, "cls_ControlPlot");

}
