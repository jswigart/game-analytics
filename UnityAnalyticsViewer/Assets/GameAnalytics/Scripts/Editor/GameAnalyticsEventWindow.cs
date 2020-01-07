using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using UnityEngine;
using UnityEditor;
using Google.Protobuf.Reflection;

public class GameAnalyticsEventWindow : EditorWindow
{
    Vector2 _eventScrollPosition = Vector2.zero;

    // Add menu named "My Window" to the Window menu
    [MenuItem("Game Analytics/Events")]
    static void Init()
    {
        // Get existing open window or if none, make a new one:
        GameAnalyticsEventWindow window = (GameAnalyticsEventWindow)EditorWindow.GetWindow(typeof(GameAnalyticsEventWindow));
        window.Show();
    }

    void OnGUI()
    {
        EditorGUILayout.LabelField("Game Analytics Events", EditorStyles.boldLabel);

        var ga = GameAnalytics.Instance;
        if (ga == null)
            return;

        var activeEvent = ga.ActiveEventLayer;

        GUILayout.BeginHorizontal("box");
        GUILayout.FlexibleSpace();
        GUILayout.Label("Game Analytics Events");
        GUILayout.FlexibleSpace();
        GUILayout.EndHorizontal();

        _eventScrollPosition = GUILayout.BeginScrollView(_eventScrollPosition);
        
        foreach (var ekv in ga.EventViewers)
        {
            Color defaultColor = GUI.color;
            GUI.color = ekv.Value.EventDescriptor == activeEvent ? Color.green : Color.red;
            GUILayout.BeginHorizontal("box");
            GUI.color = defaultColor;

            if(GUILayout.Button("Select"))
            {
                ga.SetActiveEventLayer(ekv.Value.EventDescriptor);
            }
            GUILayout.Label(ekv.Value.EventDescriptor.Name);
            GUILayout.Label(string.Format("({0}) Events", ekv.Value.Events.Count));
            GUILayout.FlexibleSpace();
            if (GUILayout.Button("Hide All"))
            {
                foreach (var fkv in ekv.Value.FieldFilter)
                    foreach (var ukv in fkv.Value.UniqueValues)
                        ukv.Value.Show = false;
                ga.ResetEventTextures();
            }
            if (GUILayout.Button("Show All"))
            {
                foreach (var fkv in ekv.Value.FieldFilter)
                    foreach (var ukv in fkv.Value.UniqueValues)
                        ukv.Value.Show = true;
                ga.ResetEventTextures();
            }
            GUILayout.EndHorizontal();

            GUILayout.BeginVertical();
            foreach (var fkv in ekv.Value.FieldFilter)
            {
                GUILayout.BeginHorizontal();
                GUILayout.Label(fkv.Key.Name);

                foreach (var uniqueValues in fkv.Value.UniqueValues)
                {
                    defaultColor = GUI.color;
                    if(uniqueValues.Value != null)
                    {
                        GUI.color = uniqueValues.Value.Show ? Color.green : Color.red;

                        GUILayout.BeginHorizontal("box");
                        if (GUILayout.Button(uniqueValues.Value.DisplayString))
                        {
                            uniqueValues.Value.Show = !uniqueValues.Value.Show;
                            ga.ResetEventTextures();
                        }
                        GUILayout.EndHorizontal();

                        GUI.color = defaultColor;
                    }
                }
                GUILayout.FlexibleSpace();
                GUILayout.EndHorizontal();
            }
            GUILayout.EndVertical();
        }

        GUILayout.EndScrollView();

        //EditorGUILayout.BeginVertical(GUILayout.Width(500.0f));
        //foreach(var ekv in ga.EventViewers)
        //{
        //    if (EditorGUILayout.BeginToggleGroup(ekv.Value.Descriptor.Name, ekv.Value.Descriptor == activeEvent))
        //    {
        //        ga.SetActiveEventLayer(ekv.Value.Descriptor);

        //        EditorGUI.indentLevel++;
        //        foreach (var fkv in ekv.Value.FieldFilter)
        //        {
        //            EditorGUILayout.LabelField(fkv.Key.JsonName, EditorStyles.boldLabel);

        //            EditorGUI.indentLevel++;
        //            foreach(var uniqueValues in fkv.Value.UniqueValues)
        //            {
        //                bool selected = EditorGUILayout.Toggle(string.Format("{0}: {1}", fkv.Key.JsonName, uniqueValues.Value.DisplayString), uniqueValues.Value.Show);
        //                if(selected != uniqueValues.Value.Show)
        //                {
        //                    uniqueValues.Value.Show = selected;
        //                    ga.ResetEventTextures();
        //                }
        //            }
        //            EditorGUI.indentLevel--;
        //        }
        //        EditorGUI.indentLevel--;
        //    }
        //    EditorGUILayout.EndToggleGroup();
        //}
        //EditorGUILayout.EndVertical();
    }
    
    private void Update()
    {
        var ga = GameAnalytics.Instance;
        if (ga != null)
        {
            if(ga.EventUIDirty)
            {
                Repaint();
            }
        }
    }
}