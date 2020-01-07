using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using StackExchange.Redis;
using System.Threading.Tasks;
using UnityEngine.Playables;

public partial class GameAnalytics : MonoBehaviour
{
    public string ActiveEventStream { get; protected set; }
    public string[] AllEventStreams { get; protected set; }
    
    Dictionary<string, GameAnalyticsNode> SceneNodes = new Dictionary<string, GameAnalyticsNode>();
    Dictionary<int, GameAnalyticsEntity> EntityNodes = new Dictionary<int, GameAnalyticsEntity>();
    
    public static GameAnalytics Instance { get; protected set; }

    private PlayableDirector _director;

    GameAnalytics()
    {
        Instance = this;
    }

    private void Start()
    {
        _director = GetComponent<PlayableDirector>();
    }

    private void OnEnable()
    {
        StartRedis();

        SetColorGradient();
    }

    private void OnDestroy()
    {
        CloseRedis();

        Instance = null;
    }
}
