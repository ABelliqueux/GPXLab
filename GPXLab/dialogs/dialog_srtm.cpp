/****************************************************************************
 *   Copyright (c) 2014 - 2015 Frederic Bourgeois <bourgeoislab@gmail.com>  *
 *                                                                          *
 *   This program is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation, either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   This program is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with This program. If not, see <http://www.gnu.org/licenses/>.   *
 ****************************************************************************/

#include "dialog_srtm.h"
#include "ui_dialog_srtm.h"
#include "qutils.h"

Dialog_srtm::Dialog_srtm(const GPX_wrapper *gpxmw, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog_srtm),
    gpxmw(gpxmw)
{
    // create new SRTM class
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // Let's use OneArcSecond files
    srtm = new SRTM(SRTM::SRTMModel::OneArcSecond);
    srtm->setDirectory(dir.toStdString());
    QDir tempDir;
    tempDir.mkpath(dir);

    // setup UI
    ui->setupUi(this);
    ui->labelDownload->setTextFormat(Qt::RichText);
    ui->labelDownload->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->labelDownload->setOpenExternalLinks(true);
    ui->labelDownloadLocation->setTextFormat(Qt::RichText);
    ui->labelDownloadLocation->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->labelDownloadLocation->setOpenExternalLinks(true);
    ui->widgetDownload->setVisible(false);
    ui->labelKernelSize->setText(QString::number(ui->horizontalSliderKernelSize->value()));
    ui->labelNumPasses->setText(QString::number(ui->horizontalSliderNumPasses->value()));

    // add original curve
    ui->widgetPlot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    ui->widgetPlot->xAxis->setDateTimeFormat("H:mm");
    ui->widgetPlot->xAxis->setDateTimeSpec(Qt::UTC);
    ui->widgetPlot->yAxis->setLabel(tr("Altitude [m]"));
    ui->widgetPlot->addGraph();
    ui->widgetPlot->addGraph();
    ui->widgetPlot->graph(0)->setPen(QPen(Qt::blue));
    ui->widgetPlot->graph(0)->setBrush(QBrush(QColor(0, 0, 255, 20)));
    ui->widgetPlot->graph(0)->setData(gpxmw->getTimeValues(), gpxmw->getAltitudeValues());
    ui->widgetPlot->graph(0)->rescaleAxes();
    ui->widgetPlot->updateExt();
    ui->widgetPlot->replot();

    setModified(false);
}

Dialog_srtm::~Dialog_srtm()
{
    delete ui;
    delete srtm;
}

void Dialog_srtm::on_Dialog_srtm_accepted()
{
    if (modified)
    {
        setModified(false);
    }
    else
    {
        setResult(QDialog::Rejected);
    }
}

const QVector<double> &Dialog_srtm::getValues() const
{
    return values;
}

void Dialog_srtm::setModified(bool modified)
{
    if (modified)
        setWindowTitle("*" + tr("Get Altitude From Database"));
    else
        setWindowTitle(tr("Get Altitude From Database"));
    this->modified = modified;
}

bool Dialog_srtm::generateSRTMAltitudeValues(int option)
{
    values.clear();
    if (gpxmw->initTrksegIteration(gpxmw->getSelectedTrackNumber()) > 0)
    {
        const GPX_trksegType* trkseg;
        const GPX_wptType* wpt;
        double altitude = 0.0;
        short altitudeSRTM = 0;
        int TrackSegmentNumber = 0;
        while ((trkseg = gpxmw->getNextTrkseg()))
        {
            if (gpxmw->initTrkptIteration(gpxmw->getSelectedTrackNumber(), TrackSegmentNumber) > 0)
            {
                while ((wpt = gpxmw->getNextTrkpt()))
                {
                    switch (option)
                    {
                    case 0:
                        if (srtm->getAltitude(wpt->latitude, wpt->longitude, altitudeSRTM) == false)
                            return false;
                        altitude = altitudeSRTM;
                        break;

                    case 1:
                        if (wpt->altitude == 0.0)
                        {
                            if (srtm->getAltitude(wpt->latitude, wpt->longitude, altitudeSRTM) == false)
                                return false;
                            altitude = altitudeSRTM;
                        }
                        else
                        {
                            altitude = wpt->altitude;
                        }
                        break;

                    case 2:
                        if (srtm->getAltitude(wpt->latitude, wpt->longitude, altitudeSRTM) == false)
                            return false;
                        altitude = wpt->altitude / altitudeSRTM;
                        if (altitude < 0.9 || altitude > 1.1)
                            altitude = altitudeSRTM;
                        else
                            altitude = wpt->altitude;
                        break;
                    }
                    values.append((double)altitude);
                }
            }
            TrackSegmentNumber++;
        }
        setModified(true);
        return true;
    }
    return false;
}

void Dialog_srtm::on_pushButtonFetchData_clicked()
{
    int option = 0;
    if (ui->radioButton_1->isChecked())
        option = 0;
    else if (ui->radioButton_2->isChecked())
        option = 1;
    else if (ui->radioButton_3->isChecked())
        option = 2;

    // generate altitude values from SRTM database
    if (generateSRTMAltitudeValues(option))
    {
        ui->widgetDownload->setVisible(false);

        // average data
        for (int n = 0; n < ui->horizontalSliderNumPasses->value(); ++n)
            QUtils::movingAverage(values, ui->horizontalSliderKernelSize->value());

        // SRTM curve
        ui->widgetPlot->graph(1)->clearData();
        ui->widgetPlot->graph(1)->setPen(QPen(Qt::red));
        ui->widgetPlot->graph(1)->setData(gpxmw->getTimeValues(), values);
        ui->widgetPlot->graph(1)->rescaleAxes();
        ui->widgetPlot->replot();
    }
    else
    {
        QString url = QString::fromStdString(srtm->getFileURL());
        QString dir = QString::fromStdString(srtm->getDirectory());
        ui->labelNotFound->setText(tr("Height file not found: ") + QString::fromStdString(srtm->getFileName()));
        ui->labelDownload->setText(tr("Download file here: ") + "<a href=\"" + url + "\">" + url + "</a>");
        ui->labelDownloadLocation->setText(tr("Unzip and put file here: ") + "<a href=\"" + QUrl::fromLocalFile(dir).toString() + "\">" + dir + "</a>");
        ui->widgetDownload->setVisible(true);
    }
}

void Dialog_srtm::on_horizontalSliderKernelSize_valueChanged(int value)
{
    ui->labelKernelSize->setText(QString::number(value));
}

void Dialog_srtm::on_horizontalSliderNumPasses_valueChanged(int value)
{
    ui->labelNumPasses->setText(QString::number(value));
}
