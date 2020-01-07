using System;
using System.Linq;
using System.Reflection;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using Google.Protobuf;

public class GameAnalyticsProtobufEditor : Editor
{
    List<Analytics.EditorChangeValue> _changes = new List<Analytics.EditorChangeValue>();

    protected void InspectProtobuf(string varname)
    {
        _changes.Clear();

        // keep track of the count we actually made during the only iteration that is allowed to add changes
        // then propagate up to that count to the other targets
        int numChanges = 0;

        for(int t = 0; t < targets.Length; ++t)
        {
            var pi = targets[t].GetType().GetProperty(varname);
            if(pi != null && pi.CanRead)
            {
                var message = pi.GetValue(targets[t]) as IMessage;
                if (message != null)
                {
                    if (t == 0)
                    {
                        EditorGUILayout.BeginVertical(EditorStyles.helpBox);
                        ProtobufUtilities.ShowProtoData(message, _changes);
                        EditorGUILayout.EndVertical();

                        numChanges = _changes.Count;
                    }
                    else if (numChanges > 0)
                    {
                        // propagate these changes to the other selected targets
                        for (int c = 0; c < numChanges; ++c)
                        {
                            ProtobufUtilities.PropagateChange(message, _changes[c], _changes);
                        }
                    }
                }
            }
            else
            {
                Debug.LogErrorFormat("InspectProtobuf: No property by the name {0}", varname);
            }
        }
        
        if (_changes.Count > 0)
        {
            GameAnalytics.Instance.PublishChangesToServer(_changes);
        }
    }
}
