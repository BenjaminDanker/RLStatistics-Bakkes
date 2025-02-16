#include "pch.h"
#include "RLStatistics.h"
#include <cmath>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <Python.h>
#include <iostream>
#include <string>
#include "CPPRP/ReplayFile.h"




BAKKESMOD_PLUGIN(RLStatistics, "RLStatistics", plugin_version, PERMISSION_ALL)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;


void ExtractXPositionFromReplay(const std::string& replayPath)
{
    try {
        CPPRP::ReplayFile replay(replayPath);
        replay.Load();
        replay.DeserializeHeader();
        replay.Parse();

        for (const auto& tick : replay.frames) {
            tick.time;
        }

        // Load replay using CPPRP
        auto replayFile = std::make_shared<CPPRP::ReplayFile>(replayPath);
        replayFile->Load();
        replayFile->DeserializeHeader();
        replayFile->tickables.push_back([&](const CPPRP::Frame& f, const std::unordered_map<uint32_t, CPPRP::ActorStateData>& actorStats)
            {
                std::cout << f.frameNumber << " " << f.time << "\n";
                for (auto& actor : actorStats)
                {
                    std::shared_ptr<CPPRP::TAGame::Car_TA> car = std::dynamic_pointer_cast<CPPRP::TAGame::Car_TA>(actor.second.actorObject);
                    if (car)
                    {
                        auto rbState = car->ReplicatedRBState;
                        std::cout << "   Car: " << actor.second.actorId << " " << actor.second.nameId << " " << car->PlayerReplicationInfo.actor_id << " ";
                        std::cout << rbState.position.ToString() << "\n";
                    }
                }
            });
        replayFile->Parse();

    }
    catch (const std::exception& e) {
        std::cerr << "Error processing replay: " << e.what() << std::endl;
    }
}




void setPythonPath(const std::string& path) {
    std::string command =
        "import sys\n"
        "sys.path.append('" + path + "')\n";
    PyRun_SimpleString(command.c_str());
}

void redirectStd() {
    std::string command =
        "import sys\n"
        "import io\n"
        "import __main__\n"
        "__main__.captured_output = io.StringIO()\n"
        "sys.stdout = __main__.captured_output\n";
    if (PyRun_SimpleString(command.c_str()) != 0) {
        PyErr_Print();
        LOG("Failed to redirect stdout.");
    }
}


void callPythonFunctionAndLog(const std::string& moduleName, const std::string& functionName) {
    try {
        PyObject* mainModule = PyImport_AddModule("__main__"); // Access the main module
        if (!mainModule) {
            LOG("Failed to access main module.");
        }

        // Load the module (e.g., 'main' is the script name without '.py' extension)
        PyObject* pModule = PyImport_ImportModule(moduleName.c_str());
        if (!pModule) {
            PyErr_Print();
            LOG("Failed to load Python module: " + moduleName);
            return;
        }

        // Get the function from the module
        PyObject* pFunc = PyObject_GetAttrString(pModule, functionName.c_str());
        if (!pFunc || !PyCallable_Check(pFunc)) {
            PyErr_Print();
            LOG("Failed to load or call Python function: " + functionName);
            Py_XDECREF(pFunc);
            Py_XDECREF(pModule);
            return;
        }

        // Call the Python function and expect a string return value
        PyObject* pResult = PyObject_CallObject(pFunc, nullptr);
        if (!pResult) {
            PyErr_Print();  // Print any Python error
            LOG("Python function execution failed.");
        }
        else {
            // Extract the returned string from the Python function
            const char* output = PyUnicode_AsUTF8(pResult);
            if (output) {
                LOG(output);  // Log the output to BakkesMod
            }
            else {
                LOG("Failed to convert Python result to string.");
            }
        }

        // Print out python's print statements
        PyObject* capturedOutput = PyObject_GetAttrString(mainModule, "captured_output");
        if (capturedOutput) {
            PyObject* capturedValue = PyObject_CallMethod(capturedOutput, "getvalue", nullptr);
            if (capturedValue && PyUnicode_Check(capturedValue)) {
                const char* outputStr = PyUnicode_AsUTF8(capturedValue);
                LOG("Captured output: " + std::string(outputStr));
                Py_DECREF(capturedValue);
            }
            else {
                LOG("Failed to retrieve captured output.");
            }
            Py_XDECREF(capturedOutput);
        }
        else {
            LOG("Failed to access captured_output.");
        }

        // Clean up references
        Py_XDECREF(pResult);
        Py_XDECREF(pFunc);
        Py_XDECREF(pModule);
    }
    catch (const std::exception& e) {
        LOG("Error during Python function call: " + std::string(e.what()));
    }
}




void RLStatistics::onLoad()
{
    _globalCvarManager = cvarManager;
    isRecording = false;
    isInReplay = false;
    gameMode = 0;
    isPathAppend = false;

    if (!Py_IsInitialized()) {
        Py_Initialize();
        LOG("Python interpreter initialized.");
    }
    std::string scriptDirectory = "D:\\Coding\\Python\\RLStatistics";
    setPythonPath(scriptDirectory);
    redirectStd();


    LOG("RLStatistics Plugin loaded!");
    //Function TAGame.GameMode_TA.GetLocalizedName
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound", [this](std::string eventName) {
        callPythonFunctionAndLog("main", "run");


        if (isRecording) {
            LOG("Already recording");
            return; // Prevent duplicate recordings
        }

        MMRWrapper mw = gameWrapper->GetMMRWrapper();
        gameMode = mw.GetCurrentPlaylist();
        if (!(std::find(std::begin(playlists), std::end(playlists), gameMode) != std::end(playlists))) {
            LOG("Not Ranked or Private Match");
            return;
        }
        
        LOG("GameMode: " + std::to_string(gameMode));
        LOG("Match officially started! Starting recording.");
        isRecording = true;
        previousBlueScore = 0;
        previousOrangeScore = 0;
        StartTimer();
        GenerateMatchID();
        });

    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.TriggerGoalScoreEvent", [this](std::string eventName) {
        RecordGoalEvent();
        });

    /*gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetAttackerPRI", [this](CarWrapper car, void* params, std::string eventName) {
        LOG("Hook triggered: SetAttackerPRI");
        RecordDemoEvent(car, params);
        });*/


    gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.OnHitBall", [this](CarWrapper car, void*, std::string) {
        RecordBallHitEvent(car);
        });

    gameWrapper->HookEvent("Function ReplayDirector_TA.Playing.BeginState", [this](std::string eventName) {
        LOG("Replay started. Skipping data recording.");
        isInReplay = true;
        });

    gameWrapper->HookEvent("Function ReplayDirector_TA.Playing.EndState", [this](std::string eventName) {
        LOG("Replay started. Skipping data recording.");
        isInReplay = false;
        });

    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", [this](std::string eventName) {
        StopRecording();
        });

    // Hook player leaving the match early
    gameWrapper->HookEvent("Function TAGame.GFxShell_TA.LeaveMatch", [this](std::string eventName) {
        LOG("Match destroyed. Stopping recording.");
        StopRecording();
        });
}

void RLStatistics::onUnload() {
    if (Py_IsInitialized()) {
        Py_Finalize();
        LOG("Python interpreter finalized.");
    }
}


// TODO
// data for other players is always random, see if there is a way to get actual values
// test carID
// figure out why python print isn't going into bakkes LOG

void RLStatistics::StartTimer()
{
    if (!isRecording) return;

    auto serverWrapper = gameWrapper->GetCurrentGameState();
    if (!serverWrapper) {
        LOG("Server wrapper is still null. Retrying...");
        gameWrapper->SetTimeout([this](GameWrapper* gw) {
            StartTimer(); // Retry after a short delay
            }, 0.5f);
        return;
    }

    gameWrapper->SetTimeout([this](GameWrapper* gw) {
        if (!isInReplay) {
            RecordData();
            //LogAllEvents();
            MakeRow();
        }
        else {
            LOG("Skipping data recording: In replay mode.");
        }
        StartTimer(); // Continue timer loop
        }, 1.0f);
}

void RLStatistics::StopRecording()
{
    if (!isRecording) return; // Prevent duplicate cleanup

    LOG("Stopping recording and resetting state.");
    isRecording = false;
    MatchOver(); // Perform final data logging and cleanup
}

void RLStatistics::RecordData()
{
    auto currentGameState = gameWrapper->GetCurrentGameState();
    if (!currentGameState) {
        LOG("Current game state is null. Cannot record positions.");
        return;
    }


    auto curball = currentGameState.GetBall();
    if (!curball) {
        LOG("Ball data is unavailable.");
    }
    else {
        std::vector<Ball> currentBall;

        auto ballLocation = curball.GetLocation();
        auto ballVelocity = curball.GetVelocity();
        auto ballAngularVelocity = curball.GetAngularVelocity();
        float ballSpeed = sqrtf(ballVelocity.X * ballVelocity.X + ballVelocity.Y * ballVelocity.Y + ballVelocity.Z * ballVelocity.Z);

        ball.push_back({
            ballLocation.X, ballLocation.Y, ballLocation.Z,
            ballVelocity.X, ballVelocity.Y, ballVelocity.Z,
            ballSpeed,
            ballAngularVelocity.X, ballAngularVelocity.Y, ballAngularVelocity.Z,
            CalculateDistance(ballLocation, { 0, 0, 0 }), // Distance to Blue Goal (replace with actual goal location if needed)
            CalculateDistance(ballLocation, { 0, 5120, 0 }) // Distance to Orange Goal (replace with actual goal location if needed)
        });
    }

    auto cars = currentGameState.GetCars();
    if (cars.Count() == 0) {
        LOG("No cars in the game. Cannot record positions.");
    }
    else {
        std::vector<Player> currentPlayers;
        for (int i = 0; i < cars.Count(); ++i)
        {
            // Player data
            auto car = cars.Get(i);
            if (!car) continue;

            auto pri = car.GetPRI();
            if (!pri) continue;

            std::string playerName = car.GetOwnerName();
            auto carID = car.GetLoadoutBody();
            int teamNum = pri.GetTeamNum();
            auto location = car.GetLocation();
            auto velocity = car.GetVelocity();
            float speed = sqrtf(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z);
            Rotator rotation = car.GetRotation();
            /*
            bool isSuperSonic = (bool) car.GetbSuperSonic();
            bool isOnWall = car.IsOnWall();
            bool isOnGround = car.IsOnGround();
            bool isInGoal = (bool)car.GetbWasInGoalZone();
            bool isDodging = car.IsDodging();
            bool hasFlip = (bool)car.HasFlip();
            */

            float currentBoost = 0.0f;
            bool isUsingBoost = false;
            if (IsLocalPlayerCar(car)) { // if your car
                auto boost = car.GetBoostComponent();
                currentBoost = boost.IsNull() ? 0.0f : boost.GetCurrentBoostAmount();
                isUsingBoost = previousBoostAmounts[playerName] > currentBoost;
                previousBoostAmounts[playerName] = currentBoost;
            }


            auto curball = ball.back();
            currentPlayers.push_back({
                playerName, teamNum, carID,
                location.X, location.Y, location.Z,
                velocity.X, velocity.Y, velocity.Z,
                rotation.Pitch, rotation.Roll, rotation.Yaw,
                speed, currentBoost, isUsingBoost,
                CalculateDistance(location, {curball.posX, curball.posY, curball.posZ}), // distance to ball
                });
        }
        player.push_back(currentPlayers);
    }
}

void RLStatistics::RecordGoalEvent()
{
    auto serverWrapper = gameWrapper->GetCurrentGameState();

    if (!serverWrapper) {
        LOG("HandleGoalScored: Server wrapper is null");
        return;
    }

    auto teams = serverWrapper.GetTeams();
    if (teams.Count() < 2) {
        LOG("HandleGoalScored: Not enough teams");
        return;
    }

    // Fetch the scores
    int currentBlueScore = teams.Get(0).GetScore();
    int currentOrangeScore = teams.Get(1).GetScore();

    // Determine which team scored
    if (currentBlueScore > previousBlueScore) {
        eventLogs.emplace_back(Event::GOAL, 0);
        LOG("Goal scored on Orange team by Blue team.");
    }
    else if (currentOrangeScore > previousOrangeScore) {
        eventLogs.emplace_back(Event::GOAL, 1);
        LOG("Goal scored on Blue team by Orange team.");
    }

    // Update previous scores
    previousBlueScore = currentBlueScore;
    previousOrangeScore = currentOrangeScore;
}


void RLStatistics::RecordDemoEvent(CarWrapper car, void* params)
{
    if (!car) {
        LOG("RecordDemoWithAttacker: Car is null");
        return;
    }

    auto attackerPRI = car.GetPRI();
    std::string attackerName = attackerPRI.IsNull() ? "Unknown Victim" : attackerPRI.GetPlayerName().ToString();

    // Add the demo event to eventLogs
    eventLogs.emplace_back(Event::DEMO, -1, attackerName);
}

void RLStatistics::RecordBallHitEvent(CarWrapper car)
{
    if (!car) {
        LOG("RecordDemoWithAttacker: Car is null");
        return;
    }

    eventLogs.emplace_back(Event::BALL_HIT, -1, car.GetOwnerName());
}

void RLStatistics::LogAllEvents()
{
    std::ostringstream logStream;

    // Log the latest event
    if (!eventLogs.empty())
    {
        const auto& latestEvent = eventLogs.back();
        logStream << "---- Latest Event ----\n";
        switch (latestEvent.type)
        {
        case Event::GOAL:
            logStream << "Goal Scored: Team " + latestEvent.playerName + (latestEvent.teamNum == 0 ? "Blue" : "Orange") << "\n";
            break;

        case Event::DEMO:
            logStream << "Demo Event: Player " << latestEvent.playerName << " was demolished\n";
            break;

        case Event::BALL_HIT:
            logStream << "Ball Hit Event: Player " << latestEvent.playerName << " hit the ball\n";
            break;
        }
    }
    else
    {
        logStream << "No events logged.\n";
    }

    // Log the latest player data
    logStream << "---- Player Data ----\n";
    if (!player.empty())
    {
        const auto& latestPlayerFrame = player.back();
        for (const auto& pos : latestPlayerFrame)
        {
            logStream << "Player: " << pos.playerName
                << " | Team: " << (pos.teamNum == 0 ? "Blue" : "Orange")
                << " | CarID: " << pos.carID
                << " | Pos: X=" << pos.posX << ", Y=" << pos.posY << ", Z=" << pos.posZ
                << " | Speed: " << pos.speed
                << " | Boost: " << pos.boost
                << " | Using Boost: " << (pos.isUsingBoost ? "Yes" : "No")
                << " | Distance to Ball: " << pos.distanceToBall << "\n";
        }
    }
    else
    {
        logStream << "Player data is empty.\n";
    }

    // Log the latest ball data
    logStream << "---- Ball Data ----\n";
    if (!ball.empty())
    {
        const auto& latestBallData = ball.back();
        logStream << "Ball Pos: (" << latestBallData.posX << ", " << latestBallData.posY << ", " << latestBallData.posZ << ")\n"
            << "Ball Speed: " << latestBallData.speed << "\n"
            << "Ball Vel: (" << latestBallData.velX << ", " << latestBallData.velY << ", " << latestBallData.velZ << ")\n";
    }
    else
    {
        logStream << "Ball data is empty.\n";
    }

    LOG(logStream.str());
}

void RLStatistics::MakeRow()
{
    auto now = std::chrono::steady_clock::now();
    long long elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

    // Capture the most recent event, if any
    std::string eventType = "";
    std::string eventPlayer = "";
    std::string eventTeam = "";
    if (!eventLogs.empty())
    {
        const auto& event = eventLogs.back();
        if (event.type == Event::GOAL)
        {
            eventType = "Goal";
            eventPlayer = event.playerName;
            eventTeam = (event.teamNum == 0 ? "Blue" : "Orange");
        }
        else if (event.type == Event::DEMO)
        {
            eventType = "Demo";
            eventPlayer = event.playerName;
        }
        else if (event.type == Event::BALL_HIT)
        {
            eventType = "BallHit";
            eventPlayer = event.playerName;
        }
    }
    eventLogs.clear();

    // Collect player data for the most recent frame
    std::vector<std::vector<float>> playerData;
    if (!player.empty())
    {
        const auto& latestPlayerFrame = player.back(); // Get the latest frame
        for (const auto& pos : latestPlayerFrame)
        {
            playerData.push_back({
                pos.posX, pos.posY, pos.posZ,
                pos.velX, pos.velY, pos.velZ,
                pos.speed, pos.boost,
                static_cast<float>(pos.pitch), static_cast<float>(pos.roll), static_cast<float>(pos.yaw),
                pos.isUsingBoost ? 1.0f : 0.0f,
                pos.distanceToBall
                });
        }
    }
    player.clear();

    // Pad player data if fewer than 4 players
    while (playerData.size() < 4)
    {
        playerData.emplace_back(13, 0.0f); // 13 fields of zeros
    }

    // Get the most recent ball data
    std::vector<float> ballDataVector;
    if (!ball.empty())
    {
        const auto& latestBallData = ball.back();
        ballDataVector = {
            latestBallData.posX, latestBallData.posY, latestBallData.posZ,
            latestBallData.velX, latestBallData.velY, latestBallData.velZ,
            latestBallData.speed,
            latestBallData.rotVelX, latestBallData.rotVelY, latestBallData.rotVelZ,
            latestBallData.distanceToBlueGoal, latestBallData.distanceToOrangeGoal
        };
    }
    ball.clear();

    // Create and store the row
    MatchRow row = { elapsedTime, matchID, eventType, eventPlayer, eventTeam, playerData, ballDataVector };
    matchRows.push_back(row);
}

void RLStatistics::MatchOver()
{
    //if (gameMode == 6) return; // Don't write if private match

    size_t totalPlayers = (gameMode == 6) ? 4 : (gameMode == 10) ? 2 : (gameMode == 11) ? 4 : (gameMode == 13 ? 6 : 0);

    std::string filePath;
    if (totalPlayers == 2) {
        filePath = "C:/Users/bsd20/source/repos/RLStatistics/plugins/RLStatistics_1v1.csv";
    }
    else if (totalPlayers == 4) {
        filePath = "C:/Users/bsd20/source/repos/RLStatistics/plugins/RLStatistics_2v2.csv";
    }
    else if (totalPlayers == 6) {
        filePath = "C:/Users/bsd20/source/repos/RLStatistics/plugins/RLStatistics_3v3.csv";
    }
    else {
        LOG("Invalid Total Players");
        return;
    }

    bool fileExists = std::filesystem::exists(filePath);

    std::ofstream outFile(filePath, std::ios::app);
    if (!outFile.is_open())
    {
        LOG("Failed to open file for writing!");
        return;
    }

    if (!fileExists)
    {
        LOG("Creating new file");

        outFile << "MatchID,Timestamp,EventType,EventPlayer,EventTeam";
        for (size_t i = 1; i <= totalPlayers; ++i)
        {
            outFile << ",Player" << i << "_PosX,Player" << i << "_PosY,Player" << i << "_PosZ,"
                    << "Player" << i << "_VelX,Player" << i << "_VelY,Player" << i << "_VelZ,"
                    << "Player" << i << "_Speed,Player" << i << "_Boost,Player" << i << "_Pitch,"
                    << "Player" << i << "_Roll,Player" << i << "_Yaw,Player" << i << "_UsingBoost,"
                    << "Player" << i << "_DistanceToBall";
        }
        outFile << ",Ball_PosX,Ball_PosY,Ball_PosZ,Ball_VelX,Ball_VelY,Ball_VelZ,Ball_Speed,"
                << "Ball_RotVelX,Ball_RotVelY,Ball_RotVelZ,Ball_DistanceToBlueGoal,Ball_DistanceToOrangeGoal\n";
    }

    // Write rows
    for (const auto& row : matchRows)
    {
        // Write general match data
        outFile << row.matchID << "," << row.timestamp << "," << row.eventType << "," << row.playerName << "," << row.playerTeam;

        for (size_t i = 0; i < totalPlayers; ++i)
        {
            const auto& playerFrame = row.playerData[i];
            for (const auto& value : playerFrame)
            {
                outFile << "," << value;
            }
        }

        // Write ball data
        for (const auto& ballValue : row.ballData)
        {
            outFile << "," << ballValue;
        }

        // End the row
        outFile << "\n";
    }

    // Clear match data after writing
    matchRows.clear();

    outFile.close();

    LOG("Match data written to: " + filePath);
}





// Util
void RLStatistics::GenerateMatchID()
{
    // Generate a random match ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(100000, 999999);

    matchID = std::to_string(dist(gen));
    LOG("Generated Match ID: " + matchID);
}

bool RLStatistics::IsLocalPlayerCar(CarWrapper car)
{
    if (!car) return false;

    auto localPlayer = gameWrapper->GetLocalCar();
    if (!localPlayer) return false;

    auto localPRI = localPlayer.GetPRI();
    auto carPRI = car.GetPRI();

    if (localPRI.GetPlayerName().ToString() == carPRI.GetPlayerName().ToString()) {
        return true;
    }
    else {
        return false;
    }
}

float RLStatistics::CalculateDistance(const Vector& a, const Vector& b)
{
    float dx = a.X - b.X;
    float dy = a.Y - b.Y;
    float dz = a.Z - b.Z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}