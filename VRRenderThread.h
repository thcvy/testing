
#ifndef VR_RENDER_THREAD_H
#define VR_RENDER_THREAD_H

/* Qt headers */
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

/* Vtk headers */
#include <vtkActor.h>
#include <vtkSmartPointer.h>
#include <vtkOpenVRRenderWindow.h>
#include <vtkOpenVRRenderWindowInteractor.h>
#include <vtkOpenVRRenderer.h>
#include <vtkOpenVRCamera.h>
#include <vtkActorCollection.h>
#include <vtkCommand.h>

#include <chrono>



class VRRenderThread : public QThread {
    Q_OBJECT

public:
    /** List of command names */
    enum {
        END_RENDER,
        ROTATE_X,
        ROTATE_Y,
        ROTATE_Z,
        RESET_VIEW,        /**< Reset all rotations to zero            */
        TOGGLE_SPIN,       /**< Toggle continuous spin animation       */
        TOGGLE_FLOOR       /**< Show / hide the floor and skybox       */
    } Command;

    /** Constructor */
    explicit VRRenderThread(QObject* parent = nullptr);

    /** Destructor */
    ~VRRenderThread() override;

    /** Add an actor BEFORE the VR interactor has been started.
     *  This is the standard way to populate the VR scene at start-up.
     */
    void addActorOffline(vtkActor* actor);

    /** Add an actor while VR is RUNNING (thread-safe).
     *  The actor is queued and added to the renderer on the next animation tick.
     */
    void addActorLive(vtkActor* actor);

    /** Issue a simple animation command (rotation rate, end render, etc).
     *  Thread-safe.
     */
    void issueCommand(int cmd, double value);

    /** Thread-safe colour update for an actor that is already in the VR scene. */
    void requestColourUpdate(vtkActor* actor,
                             double r, double g, double b);

    /** Thread-safe visibility update for an actor that is already in the VR scene. */
    void requestVisibilityUpdate(vtkActor* actor, bool visible);

    /** Returns true if VR is running. */
    bool isVRRunning() const { return running; }

protected:
    /** This is a re-implementation of a QThread function */
    void run() override;

private:
    /* Standard VTK VR Classes */
    vtkSmartPointer<vtkOpenVRRenderWindow>            window;
    vtkSmartPointer<vtkOpenVRRenderWindowInteractor>  interactor;
    vtkSmartPointer<vtkOpenVRRenderer>                renderer;
    vtkSmartPointer<vtkOpenVRCamera>                  camera;

    /* Mutex used to protect command queue / pending updates */
    QMutex                                             mutex;
    QWaitCondition                                     condition;

    /** List of actors that will need to be added to the VR scene at startup */
    vtkSmartPointer<vtkActorCollection>                actors;

    /** Floor / scenery actors (kept separately so we can toggle them off) */
    vtkSmartPointer<vtkActorCollection>                envActors;

    /** A timer to help implement animations and visual effects */
    std::chrono::time_point<std::chrono::steady_clock> t_last;

    /** When set true by GUI, the VR loop will exit. */
    bool                                               endRender;

    /** True while the run() loop is active. */
    bool                                               running;

    /* Animation state */
    double rotateX;        /*< Degrees to rotate around X axis (per time-step) */
    double rotateY;        /*< Degrees to rotate around Y axis (per time-step) */
    double rotateZ;        /*< Degrees to rotate around Z axis (per time-step) */

    /** Stored "spin" state - allows the user to pause / resume animation
     *  without losing the previously set per-axis rotation rates.            */
    bool   spinning;
    double savedRotateX;
    double savedRotateY;
    double savedRotateZ;

    /** Whether the floor/skybox is currently visible. */
    bool   showFloor;

  
    struct PendingUpdate {
        enum Kind { COLOUR, VISIBILITY, ADD_ACTOR } kind;
        vtkSmartPointer<vtkActor> actor;
        double r, g, b;
        bool   flag;
    };
    QQueue<PendingUpdate>                              pendingUpdates;

    /** A single-shot command flag for "reset view" issued from GUI. */
    bool                                               resetRequested;

    /** Helper - build the floor / skybox once VR has been initialised. */
    void buildEnvironment();

    /** Helper - drain queued GUI updates inside the VR thread. */
    void processPendingUpdates();
};

#endif
