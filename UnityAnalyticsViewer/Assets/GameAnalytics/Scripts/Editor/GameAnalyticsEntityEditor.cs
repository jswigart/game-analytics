using System;
using System.Linq;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using Google.Protobuf;

[CustomEditor(typeof(GameAnalyticsEntity))]
public class GameAnalyticsEntityEditor : GameAnalyticsProtobufEditor
{
    public override void OnInspectorGUI()
    {
        GameAnalyticsEntity comp = (GameAnalyticsEntity)target;
        
        base.DrawDefaultInspector();

        InspectProtobuf("CachedInfo");
    }
}
