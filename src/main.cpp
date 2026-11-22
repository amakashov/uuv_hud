#include "gyrohorizonwidget.h"
#include "headingwidget.h"
#include "rovhudwidget.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>

#include <chrono>
#include <cmath>
#include <functional>

namespace
{
using Area = RovHudWidget::OverlayArea;
using namespace std::chrono_literals;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(bool forceTestPattern)
        : hud(new RovHudWidget(this)),
          heading(new HeadingWidget(hud)),
          horizon(new GyroHorizonWidget(hud)),
          staleLabel(new QLabel(tr("VIDEO STALE"), hud))
    {
        setWindowTitle(tr("ROV HUD"));
        setCentralWidget(hud);
        setMinimumSize(420, 460);
        resize(960, 560);

        hud->addOverlay(QStringLiteral("heading"), heading, headingPlacement());
        hud->addOverlay(QStringLiteral("horizon"), horizon, {Area::Video, Qt::AlignCenter, {}});
        staleLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #ffe5ae; background: #422315; padding: 6px 10px; font-weight: bold; }"));
        staleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        hud->addOverlay(QStringLiteral("stale"), staleLabel,
                        {Area::Viewport, Qt::AlignRight | Qt::AlignBottom, QPoint(-12, -12)});
        hud->setOverlayVisible(QStringLiteral("stale"), false);

        heading->setDesiredHeading(desiredHeading);
        addShortcut(Qt::Key_Left, [this] { adjustHeading(-5); });
        addShortcut(Qt::Key_Right, [this] { adjustHeading(5); });
        addShortcut(Qt::Key_F2, [this] {
            headingArea = headingArea == Area::Viewport ? Area::Video : Area::Viewport;
            hud->setOverlayPlacement(QStringLiteral("heading"), headingPlacement());
        });
        addShortcut(Qt::Key_Space, [this] {
            if (testPattern) {
                patternPaused = !patternPaused;
                if (patternPaused) {
                    source->stop();
                } else {
                    source->start();
                }
            }
        });

        telemetryClock.start();
        connect(&refresh, &QTimer::timeout, this, &MainWindow::refreshView);
        refresh.start(16);

        if (forceTestPattern) {
            startTestPattern(tr("Test pattern"));
        } else if (auto webcam = createWebcamSource()) {
            statusBar()->showMessage(tr("Starting webcam…"));
            source = std::move(webcam);
            connect(source.get(), &VideoSource::failed, this, [this](const QString &error) {
                if (!testPattern) {
                    if (displayedSequence == 0 && source->latestFrame().sequence == 0) {
                        startTestPattern(tr("Webcam error: %1 — test pattern").arg(error));
                    } else {
                        statusBar()->showMessage(tr("Webcam error: %1").arg(error));
                    }
                }
            }, Qt::QueuedConnection);
            sourceStarted = std::chrono::steady_clock::now();
            source->start();
        } else {
            startTestPattern(tr("Webcam unavailable — test pattern"));
        }
    }

    ~MainWindow() override
    {
        refresh.stop();
        source->stop();
    }

private:
    RovHudWidget::OverlayPlacement headingPlacement() const
    {
        return {headingArea, Qt::AlignTop | Qt::AlignHCenter, QPoint(0, 10)};
    }

    void addShortcut(Qt::Key key, const std::function<void()> &action)
    {
        auto *shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setAutoRepeat(key == Qt::Key_Left || key == Qt::Key_Right);
        connect(shortcut, &QShortcut::activated, this, action);
    }

    void adjustHeading(double delta)
    {
        desiredHeading = std::fmod(desiredHeading + delta + 360.0, 360.0);
        heading->setDesiredHeading(desiredHeading);
    }

    void startTestPattern(const QString &message)
    {
        if (source) {
            source->stop();
        }
        testPattern = true;
        displayedSequence = 0;
        source = createTestPatternSource();
        source->start();
        statusBar()->showMessage(message);
    }

    void refreshView()
    {
        const double seconds = telemetryClock.elapsed() / 1000.0;
        heading->setHeading(345.0 + 24.0 * seconds);
        horizon->setAttitude(18.0 * std::sin(seconds * 0.8), 35.0 * std::sin(seconds * 0.55));

        const auto frame = source->latestFrame();
        const auto now = std::chrono::steady_clock::now();
        if (frame.sequence != displayedSequence) {
            if (!testPattern && displayedSequence == 0 &&
                statusBar()->currentMessage() == tr("Starting webcam…")) {
                statusBar()->showMessage(tr("Webcam"));
            }
            displayedSequence = frame.sequence;
            hud->setFrame(frame);
        }
        if (frame.sequence == 0 && !testPattern && now - sourceStarted >= 5s) {
            startTestPattern(tr("Webcam did not deliver frames — test pattern"));
        }
        const bool stale = frame.sequence != 0 && now - frame.arrival >= 1s;
        if (stale != staleVisible) {
            staleVisible = stale;
            hud->setOverlayVisible(QStringLiteral("stale"), stale);
        }
    }

    RovHudWidget *hud;
    HeadingWidget *heading;
    GyroHorizonWidget *horizon;
    QLabel *staleLabel;
    Area headingArea = Area::Viewport;
    double desiredHeading = 20.0;
    std::unique_ptr<VideoSource> source;
    std::chrono::steady_clock::time_point sourceStarted;
    quint64 displayedSequence = 0;
    bool testPattern = false;
    bool patternPaused = false;
    bool staleVisible = false;
    QElapsedTimer telemetryClock;
    QTimer refresh;
};
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption testPattern(QStringLiteral("test-pattern"),
                                        QObject::tr("Use the generated test pattern."));
    parser.addOption(testPattern);
    parser.process(application);

    MainWindow window(parser.isSet(testPattern));
    window.show();
    return application.exec();
}
