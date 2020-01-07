using System;
using System.Linq;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

[CustomEditor(typeof(GameAnalytics))]
public class GameAnalyticsEditor : Editor
{
    public override void OnInspectorGUI()
    {
        GameAnalytics comp = (GameAnalytics)target;

        int selectedStream = -1;
        if (!string.IsNullOrEmpty(comp.ActiveEventStream))
        {
            selectedStream = Array.IndexOf(comp.AllEventStreams, comp.ActiveEventStream);
        }
        
        if(comp.AllEventStreams != null)
        {
            int newStream = EditorGUILayout.Popup("ActiveEventStream", selectedStream, comp.AllEventStreams);
            if (selectedStream != newStream)
            {
                comp.ChangeEventStream(comp.AllEventStreams[newStream]);
            }
        }

        EditorGUI.BeginChangeCheck();

        base.DrawDefaultInspector();

        if (EditorGUI.EndChangeCheck())
        {
            comp.SetColorGradient();
        }
    }
}
