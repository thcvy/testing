#include "VRRenderThread.h"


#include <vtkActor.h>
#include <vtkOpenVRRenderWindow.h>
#include <vtkOpenVRRenderWindowInteractor.h>
#include <vtkOpenVRRenderer.h>
#include <vtkOpenVRCamera.h>

#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkNamedColors.h>
#include <vtkCylinderSource.h>
#include <vtkPlaneSource.h>
#include <vtkCubeSource.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkSTLReader.h>
#include <vtkCallbackCommand.h>
#include <vtkLight.h>
#include <vtkTransform.h>


VRRenderThread::VRRenderThread(QObject* parent)
    : QThread(parent)
{
    /* Initialise actor lists */
    actors    = vtkSmartPointer<vtkActorCollection>::New();
    envActors = vtkSmartPointer<vtkActorCollection>::New();

    /* Initialise command variables */
    rotateX        = 0.0;
    rotateY        = 0.0;
    rotateZ        = 0.0;

    spinning       = false;
    savedRotateX   = 0.0;
    savedRotateY   = 0.0;
    savedRotateZ   = 0.0;

    showFloor      = true;
    endRender      = false;
    running        = false;
    resetRequested = false;
}


VRRenderThread::~VRRenderThread()
{

}


void VRRenderThread::addActorOffline(vtkActor* actor)
{
    if (!actor) return;


    if (!this->isRunning()) {
        actors->AddItem(actor);
    }
}

void VRRenderThread::addActorLive(vtkActor* actor)
{
    if (!actor) return;

    QMutexLocker lock(&mutex);
    PendingUpdate u;
    u.kind  = PendingUpdate::ADD_ACTOR;
    u.actor = actor;
    u.r = u.g = u.b = 0.0;
    u.flag  = false;
    pendingUpdates.enqueue(u);
}

void VRRenderThread::requestColourUpdate(vtkActor* actor,
                                         double r, double g, double b)
{
    if (!actor) return;

    QMutexLocker lock(&mutex);
    PendingUpdate u;
    u.kind  = PendingUpdate::COLOUR;
    u.actor = actor;
    u.r = r; u.g = g; u.b = b;
    u.flag  = false;
    pendingUpdates.enqueue(u);
}

void VRRenderThread::requestVisibilityUpdate(vtkActor* actor, bool visible)
{
    if (!actor) return;

    QMutexLocker lock(&mutex);
    PendingUpdate u;
    u.kind  = PendingUpdate::VISIBILITY;
    u.actor = actor;
    u.r = u.g = u.b = 0.0;
    u.flag  = visible;
    pendingUpdates.enqueue(u);
}


void VRRenderThread::issueCommand(int cmd, double value)
{
    QMutexLocker lock(&mutex);

    switch (cmd) {
    case END_RENDER:
        this->endRender = true;
        break;

    case ROTATE_X:
        this->rotateX = value;
        break;

    case ROTATE_Y:
        this->rotateY = value;
        break;

    case ROTATE_Z:
        this->rotateZ = value;
        break;

    case RESET_VIEW:

        this->resetRequested = true;
        break;

    case TOGGLE_SPIN:
        if (this->spinning) {

            this->savedRotateX = this->rotateX;
            this->savedRotateY = this->rotateY;
            this->savedRotateZ = this->rotateZ;
            this->rotateX = 0.0;
            this->rotateY = 0.0;
            this->rotateZ = 0.0;
            this->spinning = false;
        } else {

            if (savedRotateX == 0.0 && savedRotateY == 0.0 && savedRotateZ == 0.0) {
                savedRotateY = 1.0;        // default gentle Y spin
            }
            this->rotateX = this->savedRotateX;
            this->rotateY = this->savedRotateY;
            this->rotateZ = this->savedRotateZ;
            this->spinning = true;
        }
        break;

    case TOGGLE_FLOOR:
        this->showFloor = !this->showFloor;

        if (envActors) {
            envActors->InitTraversal();
            vtkActor* a;
            while ((a = envActors->GetNextActor())) {
                a->SetVisibility(this->showFloor ? 1 : 0);
            }
        }
        break;
    }
}



void VRRenderThread::buildEnvironment()
{
    if (!renderer) return;

    // Floor:
    vtkNew<vtkPlaneSource> plane;
    plane->SetOrigin(-1500, -200, -1500);
    plane->SetPoint1( 1500, -200, -1500);
    plane->SetPoint2(-1500, -200,  1500);
    plane->SetXResolution(20);
    plane->SetYResolution(20);

    vtkNew<vtkPolyDataMapper> floorMapper;
    floorMapper->SetInputConnection(plane->GetOutputPort());

    vtkNew<vtkActor> floorActor;
    floorActor->SetMapper(floorMapper);
    floorActor->GetProperty()->SetColor(0.18, 0.20, 0.25);
    floorActor->GetProperty()->SetSpecular(0.1);
    floorActor->GetProperty()->SetDiffuse(0.7);
    floorActor->GetProperty()->SetAmbient(0.2);
    floorActor->SetVisibility(showFloor ? 1 : 0);

    renderer->AddActor(floorActor);
    envActors->AddItem(floorActor);

    for (int i = 0; i < 6; ++i) {
        const double angle = i * (3.14159 / 3.0);
        const double R     = 900.0;

        vtkNew<vtkCubeSource> pillar;
        pillar->SetXLength(60);
        pillar->SetYLength(400);
        pillar->SetZLength(60);
        pillar->SetCenter(R * cos(angle), 0.0, R * sin(angle));

        vtkNew<vtkPolyDataMapper> m;
        m->SetInputConnection(pillar->GetOutputPort());

        vtkNew<vtkActor> a;
        a->SetMapper(m);
        a->GetProperty()->SetColor(0.30, 0.32, 0.40);
        a->GetProperty()->SetAmbient(0.3);
        a->SetVisibility(showFloor ? 1 : 0);

        renderer->AddActor(a);
        envActors->AddItem(a);
    }

    vtkNew<vtkLight> key;
    key->SetLightTypeToSceneLight();
    key->SetPosition( 800, 800,  800);
    key->SetFocalPoint(0, 0, 0);
    key->SetIntensity(1.0);
    renderer->AddLight(key);

    vtkNew<vtkLight> fill;
    fill->SetLightTypeToSceneLight();
    fill->SetPosition(-800, 400, -400);
    fill->SetFocalPoint(0, 0, 0);
    fill->SetIntensity(0.4);
    renderer->AddLight(fill);
}


void VRRenderThread::processPendingUpdates()
{
    QMutexLocker lock(&mutex);

    while (!pendingUpdates.isEmpty()) {
        PendingUpdate u = pendingUpdates.dequeue();
        if (!u.actor) continue;

        switch (u.kind) {
        case PendingUpdate::COLOUR:
            u.actor->GetProperty()->SetColor(u.r, u.g, u.b);
            break;

        case PendingUpdate::VISIBILITY:
            u.actor->SetVisibility(u.flag ? 1 : 0);
            break;

        case PendingUpdate::ADD_ACTOR: {
            /* Apply same orientation correction so it sits with the others */
            double* ac = u.actor->GetOrigin();
            u.actor->RotateX(-90);
            u.actor->AddPosition(-ac[0] + 0,
                                 -ac[1] - 100,
                                 -ac[2] - 200);
            renderer->AddActor(u.actor);
            break;
        }
        }
    }

    if (resetRequested) {
        resetRequested = false;


        rotateX = 0.0;
        rotateY = 0.0;
        rotateZ = 0.0;
        spinning = false;

        vtkActorCollection* actorList = renderer->GetActors();
        actorList->InitTraversal();
        vtkActor* a;
        while ((a = actorList->GetNextActor())) {
            bool isEnv = false;
            envActors->InitTraversal();
            vtkActor* e;
            while ((e = envActors->GetNextActor())) {
                if (e == a) { isEnv = true; break; }
            }
            if (isEnv) continue;

            a->SetOrientation(0, 0, 0);
            a->RotateX(-90);
        }
    }
}



void VRRenderThread::run()
{
    running = true;


    vtkNew<vtkNamedColors> colors;
    std::array<unsigned char, 4> bkg{ {26, 51, 102, 255} };
    colors->SetColor("BkgColor", bkg.data());

    renderer = vtkSmartPointer<vtkOpenVRRenderer>::New();
    renderer->SetBackground(colors->GetColor3d("BkgColor").GetData());
    renderer->SetBackground2(0.05, 0.07, 0.12);
    renderer->GradientBackgroundOn();

    {
        vtkActor* a;
        actors->InitTraversal();
        while ((a = actors->GetNextActor())) {
            renderer->AddActor(a);
        }
    }

    // Render window */
    window = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
    window->Initialize();
    window->AddRenderer(renderer);

    // OpenVR camera
    camera = vtkSmartPointer<vtkOpenVRCamera>::New();
    renderer->SetActiveCamera(camera);

    interactor = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();
    interactor->SetRenderWindow(window);
    interactor->Initialize();
    window->Render();


    {
        vtkActorCollection* actorList = renderer->GetActors();
        actorList->InitTraversal();
        vtkActor* a;
        while ((a = actorList->GetNextActor())) {
            double* ac = a->GetOrigin();
            a->RotateX(-90);
            a->AddPosition(-ac[0] + 0, -ac[1] - 100, -ac[2] - 200);
        }
    }


    buildEnvironment();

    window->Render();


    endRender = false;
    t_last    = std::chrono::steady_clock::now();

    while (!interactor->GetDone() && !this->endRender) {
        interactor->DoOneEvent(window, renderer);

        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t_last).count() > 20)
        {
            /* Apply any pending GUI updates */
            processPendingUpdates();

            /* Apply rotation animation to user actors */
            vtkActorCollection* actorList = renderer->GetActors();
            vtkActor* a;
            actorList->InitTraversal();
            while ((a = actorList->GetNextActor())) {

                bool isEnv = false;
                envActors->InitTraversal();
                vtkActor* e;
                while ((e = envActors->GetNextActor())) {
                    if (e == a) { isEnv = true; break; }
                }
                if (isEnv) continue;

                if (rotateX != 0.0) a->RotateX(rotateX);
                if (rotateY != 0.0) a->RotateY(rotateY);
                if (rotateZ != 0.0) a->RotateZ(rotateZ);
            }

            t_last = std::chrono::steady_clock::now();
        }
    }

    running = false;
}
