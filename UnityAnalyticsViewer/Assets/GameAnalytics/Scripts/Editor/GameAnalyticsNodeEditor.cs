using System;
using System.Linq;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

[CustomEditor(typeof(GameAnalyticsNode))]
public class GameAnalyticsNodeEditor : GameAnalyticsProtobufEditor
{
    public override void OnInspectorGUI()
    {
        GameAnalyticsNode comp = (GameAnalyticsNode)target;
        base.DrawDefaultInspector();

        InspectProtobuf("CachedNode");

        var nodes = comp.GetComponentsInChildren<GameAnalyticsNode>(true);
        var entities = comp.GetComponentsInChildren<GameAnalyticsEntity>(true);
        var meshes = comp.GetComponentsInChildren<GameAnalyticsMesh>(true);

        GUI.enabled = false;
        EditorGUILayout.IntField(new GUIContent("Child Nodes", "Number of nodes this child has"), nodes.Length-1);
        EditorGUILayout.IntField(new GUIContent("Child Entities", "Number of entities below this node"), entities.Length);
        EditorGUILayout.IntField(new GUIContent("Meshes", "Number of meshes below this node"), meshes.Length);
        GUI.enabled = true;

        if (GUILayout.Button("Show All"))
        {
            for (int n = 0; n < nodes.Length; ++n)
                nodes[n].gameObject.SetActive(true);
            for (int n = 0; n < entities.Length; ++n)
                entities[n].gameObject.SetActive(true);
            for (int n = 0; n < meshes.Length; ++n)
                meshes[n].gameObject.SetActive(true);
        }
        if (GUILayout.Button("Hide All"))
        {
            for (int n = 0; n < nodes.Length; ++n)
                nodes[n].gameObject.SetActive(false);
            for (int n = 0; n < entities.Length; ++n)
                entities[n].gameObject.SetActive(false);
            for (int n = 0; n < meshes.Length; ++n)
                meshes[n].gameObject.SetActive(false);
        }
    }

}
