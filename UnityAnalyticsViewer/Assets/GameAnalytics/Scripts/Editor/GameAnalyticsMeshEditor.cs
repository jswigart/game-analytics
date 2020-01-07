using System;
using System.Linq;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using Google.Protobuf;

[CanEditMultipleObjects]
[CustomEditor(typeof(GameAnalyticsMesh))]
public class GameAnalyticsMeshEditor : GameAnalyticsProtobufEditor
{
    public override void OnInspectorGUI()
    {
        GameAnalyticsMesh comp = (GameAnalyticsMesh)target;
        base.DrawDefaultInspector();

        InspectProtobuf("CachedOptions");
        InspectProtobuf("CachedMaterial");
    }
}
