#include "Logger.h"

int main(void)
{
    Logger logger;

    //----------------//
    // LOGGING EVENTS //
    //----------------//

    // Game starts
    LogEvent evStart;
    evStart.SetEventID("Game:Start");
    logger.AddLogEvent(evStart);

    // Send this event right away, since GameAnalytics measures
    //   session playtime as (time of last event) - (time of first event).
    logger.SubmitLogEvents(void);

    // Player dies
    LogEvent evDeath;
    evDeath.SetEventID("Death");
    evDeath.SetLocation(Point(23.f, 100.7f, -19.f));
    evDeath.SetArea("Level3");
    logger.AddLogEvent(evDeath);

    // Player buys some tonics
    LogEvent evBuy;
    evBuy.SetEventID("Buy:Tonic");
    evBuy.SetValue(15.f); // number of tonics bought
    logger.AddLogEvent(evBuy);

    // Once we've collected a bunch of events, submit them.
    logger.SubmitLogEvents(void);

    //------------------//
    // GETTING HEATMAPS //
    //------------------//

    // Load a heatmap of all the player deaths in level 3.
    logger.LoadHeatmap("Level3", "Death");

    // Initialize a deque of points and values.
    std::deque<std::pair<Point, int>> points;
    // Get the heatmap.
    if (logger.GetHeatmap("Level3", points))
    {
        // Classic "example function that doesn't do anything".
        DisplayHeatmap(points);
    }

    return 3; // Cause "return 0" is boring and I don't know what this would do. Probably time travel.
}
