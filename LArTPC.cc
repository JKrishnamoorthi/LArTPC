#include "G4RunManager.hh"
#include "G4UImanager.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4VUserActionInitialization.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4MuonMinus.hh"
#include "G4Step.hh"
#include "G4UserSteppingAction.hh"
#include "G4SDManager.hh"
#include "G4VModularPhysicsList.hh"
#include "FTFP_BERT.hh"
#include "G4UserEventAction.hh"

// Detector Construction
class MyDetectorConstruction : public G4VUserDetectorConstruction
{
public:
    G4VPhysicalVolume *Construct() override
    {
        auto nist = G4NistManager::Instance();

        // World volume
        auto worldMat = nist->FindOrBuildMaterial("G4_AIR");
        auto worldSolid = new G4Box("World", 2.0 * m, 2.0 * m, 2.0 * m);
        auto worldLogic = new G4LogicalVolume(worldSolid, worldMat, "World");
        auto worldPhys = new G4PVPlacement(0, {}, worldLogic, "World", 0, false, 0);

        // LArTPC Volume
        auto argonMat = nist->FindOrBuildMaterial("G4_lAr");
        auto larSolid = new G4Box("LArBox", 1.0 * m, 1.0 * m, 1.0 * m);
        auto larLogic = new G4LogicalVolume(larSolid, argonMat, "LArBox");
        new G4PVPlacement(0, {}, larLogic, "LArBox", worldLogic, false, 0);

        return worldPhys;
    }
};

// Primary Generator
class MyPrimaryGenerator : public G4VUserPrimaryGeneratorAction
{
public:
    MyPrimaryGenerator()
    {
        fParticleGun = new G4ParticleGun(1);
        auto particle = G4ParticleTable::GetParticleTable()->FindParticle("mu-");
        fParticleGun->SetParticleDefinition(particle);
        fParticleGun->SetParticleEnergy(1.0 * GeV);
        fParticleGun->SetParticleMomentumDirection(G4ThreeVector(1., 0., 0.));
        fParticleGun->SetParticlePosition(G4ThreeVector(-0.9 * m, 0., 0.));
    }

    ~MyPrimaryGenerator() override { delete fParticleGun; }

    void GeneratePrimaries(G4Event *event) override
    {
        fParticleGun->GeneratePrimaryVertex(event);
    }

private:
    G4ParticleGun *fParticleGun;
};

// Stepping Action
class MySteppingAction : public G4UserSteppingAction
{
public:
    MySteppingAction() : totalEdep(0.) {}

    void UserSteppingAction(const G4Step *step) override
    {
        auto edep = step->GetTotalEnergyDeposit();
        if (edep > 0)
        {
            totalEdep += edep;
        }
    }

    void PrintTotalEnergy()
    {
        G4cout << "Total energy deposited in event: " << totalEdep / keV << " keV" << G4endl;
    }

    void Reset()
    {
        totalEdep = 0.;
    }

private:
    G4double totalEdep;
};

class MyEventAction : public G4UserEventAction
{
public:
    MyEventAction(MySteppingAction *steppingAction)
        : fSteppingAction(steppingAction) {}

    void BeginOfEventAction(const G4Event *) override
    {
        fSteppingAction->Reset();
    }

    void EndOfEventAction(const G4Event *) override
    {
        fSteppingAction->PrintTotalEnergy();
    }

private:
    MySteppingAction *fSteppingAction;
};

// Action Initialization
class MyActionInitialization : public G4VUserActionInitialization
{
public:
    void Build() const override
    {
        auto steppingAction = new MySteppingAction();
        SetUserAction(new MyPrimaryGenerator());
        SetUserAction(steppingAction);
        SetUserAction(new MyEventAction(steppingAction));
    }
};

int main(int argc, char **argv)
{
    G4UIExecutive *ui = nullptr;
    if (argc == 1)
        ui = new G4UIExecutive(argc, argv);

    auto runManager = new G4RunManager;

    // Set up detector construction
    runManager->SetUserInitialization(new MyDetectorConstruction());

    // Physics list
    runManager->SetUserInitialization(new FTFP_BERT()); // FTFP_BERT physics list

    // Set up action initialization
    runManager->SetUserInitialization(new MyActionInitialization());

    // Initialize the run manager
    runManager->Initialize();

    // Visualization Manager
    auto visManager = new G4VisExecutive;
    visManager->Initialize();

    // Get the UI manager and set up commands
    auto UImanager = G4UImanager::GetUIpointer();
    UImanager->ApplyCommand("/vis/open TSGQT");              // OpenGL visualization
    UImanager->ApplyCommand("/vis/viewer/set/viewpointThetaPhi 90 0"); // Set initial viewpoint
    UImanager->ApplyCommand("/vis/drawVolume");                        // Draw the volume geometry
    UImanager->ApplyCommand("/vis/scene/add/trajectories smooth");     // Add smooth trajectories
    UImanager->ApplyCommand("/vis/scene/add/hits");                    // Show hits in the visualization
    UImanager->ApplyCommand("/run/beamOn 1");                          // Run the simulation for 1 event

    // Start the UI session (for interactive mode)
    if (ui)
    {
        ui->SessionStart();
        delete ui;
    }

    // Cleanup
    delete visManager;
    delete runManager;

    return 0;
}
