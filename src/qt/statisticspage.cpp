
#include "statisticspage.h"
#include "ui_statisticspage.h"
#include "main.h"
#include "wallet.h"
#include "init.h"
#include "base58.h"
#include "clientmodel.h"
#include "marscoinrpc.h"
#include <sstream>
#include <string>

using namespace json_spirit;

StatisticsPage::StatisticsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StatisticsPage)
{
    ui->setupUi(this);

   // setFixedSize(400, 420);

    connect(ui->startButton, SIGNAL(pressed()), this, SLOT(updateStatistics()));
}

int heightPrevious = -1;
int connectionPrevious = -1;
int volumePrevious = -1;
double netPawratePrevious = -1;
double pawratePrevious = -1;
double hardnessPrevious = -1;
double hardnessPrevious2 = -1;
int stakeminPrevious = -1;
int stakemaxPrevious = -1;
QString stakecPrevious = "";
QString rewardPrevious = "";


// Return average network hashes per second based on the last 'lookup' blocks,
// or from the last difficulty change if 'lookup' is nonpositive.
// If 'height' is nonnegative, compute the estimate at the time when a given block was found.
uint64_t GetNetworkHashPSL(int lookup, int height) {
    CBlockIndex *pb = pindexBest;

    if (height >= 0 && height < nBestHeight)
        pb = FindBlockByHeight(height);

    if (pb == NULL || !pb->nHeight)
        return 0;

    // If lookup is -1, then use blocks since last difficulty change.
    if (lookup <= 0)
        lookup = pb->nHeight % 2016 + 1;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight)
        lookup = pb->nHeight;

    CBlockIndex *pb0 = pb;
    int64 minTime = pb0->GetBlockTime();
    int64 maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64 time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a divide by zero exception.
    if (minTime == maxTime)
        return 0;

    uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64 timeDiff = maxTime - minTime;

    return ((boost::int64_t)(workDiff.getdouble() / timeDiff)/ 1000000 );
}

void StatisticsPage::updateStatistics()
{
    double pHardness = GetDifficulty();
    double pPawrate2 = 0.000;
    int nHeight = pindexBest->nHeight;
    uint64_t nMinWeight = 0;
    int64_t volume = 0; //((pindexBest->nMoneySupply)/100000000);
    int peers = 0;//this->model->getNumConnections();
    uint64_t pPawrate = GetNetworkHashPSL(590, nHeight);
    pPawrate2 = (double)pPawrate;

    QString height = QString::number(nHeight);
    QString phase = "";
    QString subsidy = "";
    subsidy = "50 MARS";

    QString hardness = QString::number(pHardness, 'f', 6);
    QString pawrate = QString::number(pPawrate2, 'f', 3);
    QString Qlpawrate = "";//model->getLastBlockDate().toString();

    QString QPeers = QString::number(vNodes.size());
    QString qVolume = QString::number(volume);

    if(nHeight > heightPrevious)
    {
        ui->heightBox->setText("<b><font color=\"green\">" + height + "</font></b>");
    } else {
    ui->heightBox->setText(height);
    }

    if(subsidy != rewardPrevious)
    {
        ui->rewardBox->setText("<b><font color=\"green\">" + subsidy + "</font></b>");
    } else {
    ui->rewardBox->setText(subsidy);
    }

    if(pHardness > hardnessPrevious)
    {
        ui->diffBox->setText("<b><font color=\"green\">" + hardness + "</font></b>");
    } else if(pHardness < hardnessPrevious) {
        ui->diffBox->setText("<b><font color=\"red\">" + hardness + "</font></b>");
    } else {
        ui->diffBox->setText(hardness);
    }

    if(pPawrate2 > netPawratePrevious)
    {
        ui->pawrateBox->setText("<b><font color=\"green\">" + pawrate + " MH/s</font></b>");
    } else if(pPawrate2 < netPawratePrevious) {
        ui->pawrateBox->setText("<b><font color=\"red\">" + pawrate + " MH/s</font></b>");
    } else {
        ui->pawrateBox->setText(pawrate + " MH/s");
    }

    if(Qlpawrate != pawratePrevious)
    {
        ui->localBox->setText("<b><font color=\"green\">" + Qlpawrate + "</font></b>");
    } else {
    ui->localBox->setText(Qlpawrate);
    }

    if(peers > connectionPrevious)
    {
        ui->connectionBox->setText("<b><font color=\"green\">" + QPeers + "</font></b>");
    } else if(peers < connectionPrevious) {
        ui->connectionBox->setText("<b><font color=\"red\">" + QPeers + "</font></b>");
    } else {
        ui->connectionBox->setText(QPeers);
    }

    if(volume > volumePrevious)
    {
        ui->volumeBox->setText("<b><font color=\"green\">" + qVolume + " MARS" + "</font></b>");
    } else if(volume < volumePrevious) {
        ui->volumeBox->setText("<b><font color=\"red\">" + qVolume + " MARS" + "</font></b>");
    } else {
        ui->volumeBox->setText(qVolume + " MARS");
    }
    updatePrevious(nHeight, nMinWeight, phase, subsidy, pHardness, pPawrate2, Qlpawrate, peers, volume);
}

void StatisticsPage::updatePrevious(int nHeight, int nMinWeight, QString phase, QString subsidy, double pHardness, double pPawrate2, QString Qlpawrate, int peers, int volume)
{
    heightPrevious = nHeight;
    stakeminPrevious = nMinWeight;
    stakecPrevious = phase;
    rewardPrevious = subsidy;
    hardnessPrevious = pHardness;
    netPawratePrevious = pPawrate2;
    pawratePrevious = Qlpawrate;
    connectionPrevious = peers;
    volumePrevious = volume;
}

void StatisticsPage::setModel(WalletModel *model)
{
    updateStatistics();
    this->model = model;
}


StatisticsPage::~StatisticsPage()
{
    delete ui;
}
