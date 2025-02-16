#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <random>

constexpr auto plugin_version = "1.0";

class RLStatistics : public BakkesMod::Plugin::BakkesModPlugin
{
private:
    struct Event
    {
        enum EventType { GOAL, DEMO, BALL_HIT } type;
        int teamNum;
        std::string playerName;
        std::string metadata;

        Event(EventType t, int team = -1, const std::string& name = "", const std::string& data = "")
            : type(t), teamNum(team), playerName(name), metadata(data) {
        }
    };

    struct Player
    {
        std::string playerName;
        int teamNum;
        int carID;
        float posX, posY, posZ;
        float velX, velY, velZ;
        int pitch, roll, yaw;
        float speed;
        float boost;
        bool isUsingBoost;
        float distanceToBall;
        float distanceToOwnGoal;
        float distanceToEnemyGoal;
    };

    struct Ball
    {
        float posX, posY, posZ;
        float velX, velY, velZ;
        float speed;
        float rotVelX, rotVelY, rotVelZ;
        float distanceToBlueGoal;
        float distanceToOrangeGoal;
    };

    struct MatchRow
    {
        long long timestamp;
        std::string matchID;
        std::string eventType;
        std::string playerName;
        std::string playerTeam;
        std::vector<std::vector<float>> playerData;
        std::vector<float> ballData;
    };

    // Variables to store data
    std::vector<Event> eventLogs;
    std::vector<std::vector<Player>> player;
    std::vector<Ball> ball;
    std::vector<MatchRow> matchRows;
    std::unordered_map<std::string, float> previousBoostAmounts;
    int previousBlueScore = 0;
    int previousOrangeScore = 0;
    bool isRecording = false;
    bool isInReplay = false;
    bool isPathAppend = false;
    std::string matchID;
    int gameMode = 0;
    int playlists[5] = {  
        6,  // Private
        10, // Ones
        11, // Twos
        13, // Threes
        34  // Psynoix Tournaments
    };

    // Timing
    std::chrono::time_point<std::chrono::steady_clock> startTime;

    // Methods
    void StartTimer();
    void RecordGoalEvent();
    void RecordDemoEvent(CarWrapper car, void* params);
    void RecordBallHitEvent(CarWrapper car);
    void RecordData();
    void LogAllEvents();
    void MakeRow();
    void StopRecording();
    void MatchOver();

    void GenerateMatchID();
    bool IsLocalPlayerCar(CarWrapper car);
    float CalculateDistance(const Vector& a, const Vector& b);

public:
    virtual void onLoad() override;
    virtual void onUnload() override;
};
