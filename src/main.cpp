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
#include <optional>

namespace
{
using Area = RovHudWidget::OverlayArea;
using namespace std::chrono_literals;

class MainWindow final : public QMainWindow
{
public:
    MainWindow(bool forceTestPattern, std::unique_ptr<VideoSource> gigECamera)
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
        } else if (gigECamera) {
            startCamera(std::move(gigECamera), tr("GigE camera"));
        } else if (auto webcam = createWebcamSource()) {
            webcamDeadline = std::chrono::steady_clock::now() + 5s;
            startCamera(std::move(webcam), tr("Webcam"));
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

    void startCamera(std::unique_ptr<VideoSource> camera, const QString &name)
    {
        cameraName = name;
        statusBar()->showMessage(tr("Starting %1…").arg(name));
        source = std::move(camera);
        connect(source.get(), &VideoSource::failed, this, [this](const QString &error) {
            if (testPattern) {
                return;
            }
            cameraFailed = true;
            if (source->latestFrame().sequence == 0) {
                startTestPattern(tr("%1 — test pattern").arg(error));
            } else {
                statusBar()->showMessage(tr("%1 error: %2").arg(cameraName, error));
            }
        }, Qt::QueuedConnection);
        source->start();
    }

    void refreshView()
    {
        const double seconds = telemetryClock.elapsed() / 1000.0;
        heading->setHeading(345.0 + 24.0 * seconds);
        horizon->setAttitude(18.0 * std::sin(seconds * 0.8), 35.0 * std::sin(seconds * 0.55));

        const auto frame = source->latestFrame();
        const auto now = std::chrono::steady_clock::now();
        if (frame.sequence != displayedSequence) {
            if (!testPattern && !cameraFailed && displayedSequence == 0) {
                statusBar()->showMessage(cameraName);
            }
            displayedSequence = frame.sequence;
            hud->setFrame(frame);
        }
        if (frame.sequence == 0 && !testPattern && webcamDeadline && now >= *webcamDeadline) {
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
    std::optional<std::chrono::steady_clock::time_point> webcamDeadline;
    QString cameraName;
    quint64 displayedSequence = 0;
    bool cameraFailed = false;
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
    const QCommandLineOption gigE(QStringLiteral("gige"),
                                  QObject::tr("Use a GigE Vision camera by IP address or device ID."),
                                  QStringLiteral("camera"));
    parser.addOption(testPattern);
    parser.addOption(gigE);
    parser.process(application);

    std::unique_ptr<VideoSource> gigECamera;
    if (parser.isSet(gigE)) {
        const QString cameraId = parser.value(gigE).trimmed();
        if (parser.isSet(testPattern) || cameraId.isEmpty()) {
            qCritical("--gige requires a camera address or ID and cannot be combined with --test-pattern.");
            return 2;
        }
        gigECamera = createGigECameraSource(cameraId);
        if (!gigECamera) {
            qCritical("GigE Vision support was not built. Install Aravis 0.8.25 or newer in the 0.8 series and rebuild.");
            return 2;
        }
    }

    MainWindow window(parser.isSet(testPattern), std::move(gigECamera));
    window.show();
    return application.exec();
}
