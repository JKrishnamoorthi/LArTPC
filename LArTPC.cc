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
#include "G4ThreeVector.hh"
#include "globals.hh"

// Detector Construction
class MyDetectorConstruction : public G4VUserDetectorConstruction
{
public:
    G4VPhysicalVolume *Construct() override
    {
        auto nist = G4NistManager::Instance();

        // World volume
        auto worldMat = nist->FindOrBuildMaterial("G4_AIR");
        auto worldSolid = new G4Box("World", 100.0 * m, 100.0 * m, 100.0 * m);
        auto worldLogic = new G4LogicalVolume(worldSolid, worldMat, "World");
        auto worldPhys = new G4PVPlacement(0, {}, worldLogic, "World", nullptr, false, 0);

        // LArTPC Volume
        auto argonMat = nist->FindOrBuildMaterial("G4_lAr");
        auto larSolid = new G4Box("LArBox", 65.0 * m, 12.0 * m, 12.0 * m);
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
    }

    void SetThetaPhiEnergy(double theta_deg, double phi_deg, double energy_GeV)
    {
        double theta = theta_deg * CLHEP::deg;
        double phi = phi_deg * CLHEP::deg;

        G4ThreeVector dir(
            std::sin(theta) * std::cos(phi),
            std::sin(theta) * std::sin(phi),
            std::cos(theta));

        double radius = 66.0 * m;
        G4ThreeVector pos = -dir * radius;

        fParticleGun->SetParticlePosition(pos);
        fParticleGun->SetParticleMomentumDirection(dir);
        fParticleGun->SetParticleEnergy(energy_GeV * GeV);
    }

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

    void PrintTotalEnergy() const
    {
        G4cout << "Total energy deposited: " << totalEdep / GeV << " GeV" << G4endl;
    }

    void Reset()
    {
        totalEdep = 0.;
    }

private:
    G4double totalEdep;
};

// Event Action
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
    MyActionInitialization(MyPrimaryGenerator *gen) : fGenerator(gen) {}

    void Build() const override
    {
        auto steppingAction = new MySteppingAction();
        SetUserAction(fGenerator);
        SetUserAction(steppingAction);
        SetUserAction(new MyEventAction(steppingAction));
    }

private:
    MyPrimaryGenerator *fGenerator;
};

// Main function
int main(int argc, char **argv)
{
    G4UIExecutive *ui = nullptr;
    if (argc == 1)
        ui = new G4UIExecutive(argc, argv);

    auto runManager = new G4RunManager;
    runManager->SetUserInitialization(new MyDetectorConstruction());
    runManager->SetUserInitialization(new FTFP_BERT());

    auto primaryGen = new MyPrimaryGenerator();
    runManager->SetUserInitialization(new MyActionInitialization(primaryGen));

    runManager->Initialize();

    auto visManager = new G4VisExecutive;
    visManager->Initialize();

    auto UImanager = G4UImanager::GetUIpointer();
    UImanager->ApplyCommand("/vis/open TSGQt");
    UImanager->ApplyCommand("/vis/viewer/set/viewpointThetaPhi 90 0");
    UImanager->ApplyCommand("/vis/drawVolume");
    UImanager->ApplyCommand("/vis/scene/add/trajectories smooth");
    UImanager->ApplyCommand("/vis/scene/add/hits");
    UImanager->ApplyCommand("/vis/scene/endOfEventAction accumulate 0");

    // θ-φ scan loop
    for (int theta = 0; theta <= 180; theta += 30)
    {
        for (int phi = 0; phi < 360; phi += 30)
        {
            double energy = 25.0; // GeV
            primaryGen->SetThetaPhiEnergy(theta, phi, energy);
            runManager->BeamOn(1);
            G4cout << "θ: " << theta << "°, φ: " << phi << "°, E: " << energy << " GeV" << G4endl;
        }
    }

    if (ui)
    {
        ui->SessionStart();
        delete ui;
    }

    delete visManager;
    delete runManager;

    return 0;
}
