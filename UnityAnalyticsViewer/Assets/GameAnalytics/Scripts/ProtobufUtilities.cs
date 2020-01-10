using System;
using System.Linq;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using Google.Protobuf;
using Google.Protobuf.Reflection;

public static class ProtobufUtilities
{
    static Dictionary<string, bool> ToggleGroups = new Dictionary<string, bool>();

    static bool GetToggleGroup(string name)
    {
        bool toggleState = false;
        ToggleGroups.TryGetValue(name, out toggleState);
        return toggleState;
    }
    static void SetToggleGroup(string name, bool toggle)
    {
        ToggleGroups[name] = toggle;
    }

    enum EnumType
    {
        SelectOne,
        Masked,
        Indexed,
    }

    static Analytics.EditorChangeValue GenerateEditorChangeForField(IMessage message, FieldDescriptor changedField, object changedValue)
    {
        var modifiedMessage = message.Descriptor.Parser.ParseJson(message.ToString());

        foreach (var field in modifiedMessage.Descriptor.Fields.InFieldNumberOrder())
        {
            if (field == changedField)
            {
                if (changedValue != null)
                    field.Accessor.SetValue(modifiedMessage, changedValue);
                continue;
            }

            bool editable_key;
            if (!field.TryGetOption<bool>(Analytics.AnalyticsExtensions.EditableKey, out editable_key) || !editable_key)
            {
                field.Accessor.Clear(modifiedMessage);
            }
        }

        Analytics.EditorChangeValue msg = new Analytics.EditorChangeValue();
        msg.MessageType = message.Descriptor.Name;
        msg.FieldName = changedField.Name;
        msg.Payload = modifiedMessage.ToString();
        return msg;
    }

    public static void PropagateChange(IMessage message, Analytics.EditorChangeValue change, List<Analytics.EditorChangeValue> changes)
    {
        if (message == null)
            return;

        foreach (var field in message.Descriptor.Fields.InDeclarationOrder())
        {
            bool hidden;
            if (field.TryGetOption<bool>(Analytics.AnalyticsExtensions.Hidden, out hidden) && hidden)
                continue;

            // found the field that this change applies to
            if (change.MessageType == message.Descriptor.Name && change.FieldName == field.Name)
            {
                var anotherChange = GenerateEditorChangeForField(message, field, null);

                var srcPayload = message.Descriptor.Parser.ParseJson(change.Payload);
                var myPayload = message.Descriptor.Parser.ParseJson(anotherChange.Payload);

                // need to parse out the change message to get ONLY the value field
                var myChangedField = myPayload.Descriptor.FindFieldByName(change.FieldName);

                field.Accessor.SetValue(myPayload, field.Accessor.GetValue(srcPayload));

                anotherChange.Payload = myPayload.ToString();

                changes.Add(anotherChange);
                continue;
            }

            switch (field.FieldType)
            {
                case FieldType.Message:
                    if (field.IsRepeated)
                    {
                    }
                    else
                    {
                        IMessage childMessage = (IMessage)field.Accessor.GetValue(message);
                        PropagateChange(childMessage, change, changes);
                    }
                    break;
            }
        }
    }

    static void ShowEnum<T>(List<Analytics.EditorChangeValue> changes, EnumType enumType, bool editable, IMessage message, FieldDescriptor field, string tooltip, List<KeyValuePair<string, long>> evalues, T value, Action<GUIContent, T> showValue) where T : struct,
          IComparable,
          IComparable<T>,
          IConvertible,
          IEquatable<T>,
          IFormattable
    {
        if (enumType != EnumType.SelectOne)
        {
            bool toggleState = GetToggleGroup(field.Name);

            bool enabledState = GUI.enabled;

            GUI.enabled = true;
            toggleState = EditorGUILayout.BeginToggleGroup(new GUIContent(field.Name, tooltip), toggleState);
            GUI.enabled = enabledState;

            if (toggleState)
            {
                EditorGUI.indentLevel++;

                GUI.enabled = false;
                showValue(new GUIContent("Actual Value"), value);
                GUI.enabled = enabledState;

                for (int i = 0; i < evalues.Count; ++i)
                {
                    var kv = evalues[i];
                    var longValue = value.ToInt64(null);
                    bool wasSet = false;
                    if (enumType == EnumType.Masked)
                        wasSet = (longValue & (long)kv.Value) != 0;
                    else if (enumType == EnumType.Indexed)
                        wasSet = (longValue & ((long)1 << (int)kv.Value)) != 0;

                    bool nowSet = EditorGUILayout.Toggle(new GUIContent(kv.Key), wasSet);
                    if (editable && wasSet != nowSet)
                    {
                        // handle the toggled bit slightly differently
                        if (enumType == EnumType.Masked)
                        {
                            if (nowSet)
                                longValue |= (long)kv.Value;
                            else
                                longValue &= ~(long)kv.Value;
                        }
                        else if (enumType == EnumType.Indexed)
                        {
                            if (nowSet)
                                longValue |= ((long)1 << (int)kv.Value);
                            else
                                longValue &= ~((long)1 << (int)kv.Value);
                        }

                        changes.Add(GenerateEditorChangeForField(message, field, Convert.ChangeType(longValue, typeof(T))));
                    }
                }

                EditorGUI.indentLevel--;
            }
            EditorGUILayout.EndToggleGroup();
            GUI.enabled = enabledState;

            SetToggleGroup(field.Name, toggleState);
        }
        else
        {
            int? selected = null;
            List<GUIContent> options = new List<GUIContent>();

            for (int i = 0; i < evalues.Count; ++i)
            {
                var kv = evalues[i];
                if (value.ToInt64(null) == kv.Value)
                    selected = options.Count;
                options.Add(new GUIContent(kv.Key, kv.Value.ToString()));
            }

            int newSelection = EditorGUILayout.Popup(new GUIContent(field.Name, tooltip), selected.HasValue ? selected.Value : -1, options.ToArray());
            if (editable && newSelection != selected)
            {
                changes.Add(GenerateEditorChangeForField(message, field, Convert.ChangeType(evalues[newSelection].Value, typeof(T))));
            }
        }
    }

    static string GetEnumDisplayString<T>(EnumType enumType, IMessage message, FieldDescriptor field, List<KeyValuePair<string, long>> evalues, T value) where T : struct,
          IComparable,
          IComparable<T>,
          IConvertible,
          IEquatable<T>,
          IFormattable
    {
        List<string> setValues = new List<string>();

        if (enumType != EnumType.SelectOne)
        {
            for (int i = 0; i < evalues.Count; ++i)
            {
                var kv = evalues[i];
                var longValue = value.ToInt64(null);
                bool wasSet = false;
                if (enumType == EnumType.Masked)
                    wasSet = (longValue & (long)kv.Value) != 0;
                else if (enumType == EnumType.Indexed)
                    wasSet = (longValue & ((long)1 << (int)kv.Value)) != 0;

                if (wasSet)
                {
                    setValues.Add(kv.Key);
                }
            }
        }
        else
        {
            for (int i = 0; i < evalues.Count; ++i)
            {
                var kv = evalues[i];
                if (value.ToInt64(null) == kv.Value)
                    setValues.Add(kv.Key);
            }
        }

        return string.Join(",", setValues);
    }

    public static string GetDisplayString(IMessage message, FieldDescriptor field)
    {
        List<KeyValuePair<string, long>> evalues = null;

        string enumKey;
        if (field.TryGetOption<string>(Analytics.AnalyticsExtensions.Enumkey, out enumKey))
        {
            GameAnalytics.EnumMap.TryGetValue(enumKey, out evalues);
        }

        EnumType enumType = EnumType.SelectOne;

        bool b = false;
        if (field.TryGetOption<bool>(Analytics.AnalyticsExtensions.Enumflags, out b) && b)
        {
            enumType = EnumType.Masked;
        }
        else if (field.TryGetOption<bool>(Analytics.AnalyticsExtensions.Enumflagsindexed, out b) && b)
        {
            enumType = EnumType.Indexed;
        }

        switch (field.FieldType)
        {
            case FieldType.Bytes:
                {
                    byte[] value = (byte[])field.Accessor.GetValue(message);

                    return string.Format("{0} bytes", value != null ? value.Length : 0);
                }
            case FieldType.String:
                {
                    string value = (string)field.Accessor.GetValue(message);

                    return value;
                }
            case FieldType.Bool:
                {
                    bool value = (bool)field.Accessor.GetValue(message);

                    return value.ToString();
                }
            case FieldType.Double:
                {
                    double value = (double)field.Accessor.GetValue(message);

                    return value.ToString();
                }
            case FieldType.Float:
                {
                    float value = (float)field.Accessor.GetValue(message);

                    return value.ToString();
                }
            case FieldType.Message:
                {
                    break;
                }
            case FieldType.SFixed32:
                {
                    int value = (int)field.Accessor.GetValue(message);
                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.SInt32:
                {
                    int value = (int)field.Accessor.GetValue(message);
                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.Int32:
                {
                    int value = (int)field.Accessor.GetValue(message);
                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.UInt32:
                {
                    uint value = (uint)field.Accessor.GetValue(message);

                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.Fixed32:
                {
                    uint value = (uint)field.Accessor.GetValue(message);

                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.SFixed64:
                {
                    long value = (long)field.Accessor.GetValue(message);

                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.SInt64:
                {
                    long value = (long)field.Accessor.GetValue(message);

                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.Int64:
                {
                    long value = (long)field.Accessor.GetValue(message);

                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.UInt64:
                {
                    ulong value = (ulong)field.Accessor.GetValue(message);

                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.Fixed64:
                {
                    ulong value = (ulong)field.Accessor.GetValue(message);

                    if (evalues != null)
                    {
                        return GetEnumDisplayString(enumType, message, field, evalues, value);
                    }
                    else
                    {
                        return value.ToString();
                    }
                }
            case FieldType.Enum:
                {
                    object value = field.Accessor.GetValue(message);

                    return value.ToString();
                }
        }

        return string.Format("{0}", field.FieldType);
    }

    public static void ShowProtoData(IMessage message, List<Analytics.EditorChangeValue> changes)
    {
        if (message == null)
            return;

        foreach (var field in message.Descriptor.Fields.InDeclarationOrder())
        {
            bool hidden;
            if (field.TryGetOption<bool>(Analytics.AnalyticsExtensions.Hidden, out hidden) && hidden)
                continue;

            bool editable;
            GUI.enabled = field.TryGetOption<bool>(Analytics.AnalyticsExtensions.Editable, out editable) && editable;

            string tooltip = null;
            if (field.TryGetOption<string>(Analytics.AnalyticsExtensions.Tooltip, out tooltip))
            {
            }

            List<KeyValuePair<string, long>> evalues = null;

            string enumKey;
            if (field.TryGetOption<string>(Analytics.AnalyticsExtensions.Enumkey, out enumKey))
            {
                if (!GameAnalytics.EnumMap.TryGetValue(enumKey, out evalues))
                    tooltip = string.Format("Failed To Find Enum {0}", enumKey);
            }

            EnumType enumType = EnumType.SelectOne;

            bool b = false;
            if (field.TryGetOption<bool>(Analytics.AnalyticsExtensions.Enumflags, out b) && b)
            {
                enumType = EnumType.Masked;
            }
            else if (field.TryGetOption<bool>(Analytics.AnalyticsExtensions.Enumflagsindexed, out b) && b)
            {
                enumType = EnumType.Indexed;
            }

            switch (field.FieldType)
            {
                case FieldType.Bytes:
                    {
                        byte[] value = (byte[])field.Accessor.GetValue(message);

                        GUI.enabled = false; // can't edit byte fields
                        EditorGUILayout.TagField(new GUIContent(field.Name, tooltip), string.Format("{0} bytes", value != null ? value.Length : 0));
                        break;
                    }
                case FieldType.String:
                    {
                        string value = (string)field.Accessor.GetValue(message);
                        string newValue = EditorGUILayout.TagField(new GUIContent(field.Name, tooltip), value);
                        if (editable && value != newValue)
                        {
                            changes.Add(GenerateEditorChangeForField(message, field, newValue));
                        }
                        break;
                    }
                case FieldType.Bool:
                    {
                        bool value = (bool)field.Accessor.GetValue(message);
                        bool newValue = EditorGUILayout.Toggle(new GUIContent(field.Name, tooltip), value);
                        if (editable && value != newValue)
                        {
                            changes.Add(GenerateEditorChangeForField(message, field, newValue));
                        }
                        break;
                    }
                case FieldType.Double:
                    {
                        double value = (double)field.Accessor.GetValue(message);
                        double newValue = EditorGUILayout.DoubleField(new GUIContent(field.Name, tooltip), value);
                        if (editable && value != newValue)
                        {
                            changes.Add(GenerateEditorChangeForField(message, field, newValue));
                        }
                        break;
                    }
                case FieldType.Float:
                    {
                        float value = (float)field.Accessor.GetValue(message);
                        float newValue = EditorGUILayout.FloatField(new GUIContent(field.Name, tooltip), value);
                        if (editable && value != newValue)
                        {
                            changes.Add(GenerateEditorChangeForField(message, field, newValue));
                        }
                        break;
                    }
                case FieldType.Message:
                    {
                        if (field.IsRepeated)
                        {
                        }
                        else
                        {
                            bool toggleState = GetToggleGroup(field.Name);

                            bool enabledState = GUI.enabled;
                            GUI.enabled = true;
                            toggleState = EditorGUILayout.BeginToggleGroup(new GUIContent(field.Name, tooltip), toggleState);
                            GUI.enabled = enabledState;
                            if (toggleState)
                            {
                                IMessage childMessage = (IMessage)field.Accessor.GetValue(message);
                                ShowProtoData(childMessage, changes);
                            }

                            EditorGUILayout.EndToggleGroup();
                            SetToggleGroup(field.Name, toggleState);
                        }
                        break;
                    }
                case FieldType.SFixed32:
                    {
                        int value = (int)field.Accessor.GetValue(message);
                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.IntField(c, v); });
                        }
                        else
                        {
                            EditorGUILayout.IntField(new GUIContent(field.Name, tooltip), value);
                        }
                        break;
                    }
                case FieldType.SInt32:
                    {
                        int value = (int)field.Accessor.GetValue(message);
                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.IntField(c, v); });
                        }
                        else
                        {
                            EditorGUILayout.IntField(new GUIContent(field.Name, tooltip), value);
                        }
                        break;
                    }
                case FieldType.Int32:
                    {
                        int value = (int)field.Accessor.GetValue(message);
                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.IntField(c, v); });
                        }
                        else
                        {
                            EditorGUILayout.IntField(new GUIContent(field.Name, tooltip), value);
                        }
                        break;
                    }
                case FieldType.UInt32:
                    {
                        uint value = (uint)field.Accessor.GetValue(message);

                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.IntField(c, (int)v); });
                        }
                        else
                        {
                            EditorGUILayout.IntField(new GUIContent(field.Name, tooltip), (int)value);
                        }
                        break;
                    }
                case FieldType.Fixed32:
                    {
                        uint value = (uint)field.Accessor.GetValue(message);

                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.IntField(c, (int)v); });
                        }
                        else
                        {
                            EditorGUILayout.IntField(new GUIContent(field.Name, tooltip), (int)value);
                        }
                        break;
                    }
                case FieldType.SFixed64:
                    {
                        long value = (long)field.Accessor.GetValue(message);

                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.LongField(c, v); });
                        }
                        else
                        {
                            EditorGUILayout.LongField(new GUIContent(field.Name, tooltip), value);
                        }
                        break;
                    }
                case FieldType.SInt64:
                    {
                        long value = (long)field.Accessor.GetValue(message);

                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.LongField(c, v); });
                        }
                        else
                        {
                            EditorGUILayout.LongField(new GUIContent(field.Name, tooltip), value);
                        }
                        break;
                    }
                case FieldType.Int64:
                    {
                        long value = (long)field.Accessor.GetValue(message);

                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.LongField(c, v); });
                        }
                        else
                        {
                            EditorGUILayout.LongField(new GUIContent(field.Name, tooltip), value);
                        }
                        break;
                    }
                case FieldType.UInt64:
                    {
                        ulong value = (ulong)field.Accessor.GetValue(message);

                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.LongField(c, (long)v); });
                        }
                        else
                        {
                            EditorGUILayout.LongField(new GUIContent(field.Name, tooltip), (long)value);
                        }
                        break;
                    }
                case FieldType.Fixed64:
                    {
                        ulong value = (ulong)field.Accessor.GetValue(message);

                        if (evalues != null)
                        {
                            ShowEnum(changes, enumType, editable, message, field, tooltip, evalues, value, (c, v) => { EditorGUILayout.LongField(c, (long)v); });
                        }
                        else
                        {
                            EditorGUILayout.LongField(new GUIContent(field.Name, tooltip), (long)value);
                        }
                        break;
                    }
                case FieldType.Enum:
                    {
                        object enumValue = field.Accessor.GetValue(message);
                        var enumOptions = Enum.GetValues(enumValue.GetType());
                        int selected = Array.IndexOf(enumOptions, enumValue);
                        GUIContent[] options = Enum.GetNames(enumValue.GetType()).Select((x) => new GUIContent(x)).ToArray();
                        int newSelected = EditorGUILayout.Popup(new GUIContent(field.Name, enumValue.GetType().Name), selected, options);
                        if (editable && selected != newSelected)
                        {
                            changes.Add(GenerateEditorChangeForField(message, field, newSelected));
                        }
                        break;
                    }
            }
        }

        GUI.enabled = true;
    }

    public static string ParseObjectName(IMessage message, string defaultName)
    {
        string objectName = defaultName;

        if (message.Descriptor.TryGetOption<string>(Analytics.AnalyticsExtensions.Objectname, out objectName))
        {
            string[] subVars = objectName.ParseMacros("%", "%");
            for (int i = 0; i < subVars.Length; ++i)
            {
                var field = message.Descriptor.FindFieldByName(subVars[i]);
                if (field != null)
                {
                    var value = field.Accessor.GetValue(message);
                    if (value != null)
                    {
                        string fieldString = field.Accessor.GetValue(message).ToString();

                        objectName = objectName.Replace(string.Format("%{0}%", subVars[i]), fieldString);
                    }
                }
            }
        }

        return objectName;
    }
}

public static class StringUtility
{
    public static string ReplaceCaseInsensitive(this string str, string oldValue, string newValue)
    {
        if (string.IsNullOrEmpty(oldValue))
            return str;

        int prevPos = 0;
        string retval = str;
        // find the first occurence of oldValue
        int pos = retval.IndexOf(oldValue, StringComparison.InvariantCultureIgnoreCase);

        while (pos > -1)
        {
            // remove oldValue from the string
            retval = retval.Remove(pos, oldValue.Length);

            // insert newValue in it's place
            retval = retval.Insert(pos, newValue);

            // check if oldValue is found further down
            prevPos = pos + newValue.Length;
            pos = retval.IndexOf(oldValue, prevPos, StringComparison.InvariantCultureIgnoreCase);
        }

        return retval;
    }

    public static string[] ParseMacros(this string str, string macroBeg, string macroEnd)
    {
        List<string> macros = new List<string>();

        int posStart = str.IndexOf(macroBeg, 0, StringComparison.InvariantCultureIgnoreCase);
        while (posStart > -1)
        {
            // skip the macroBeg
            posStart += macroBeg.Length;

            int posEnd = str.IndexOf(macroEnd, posStart + 1, StringComparison.InvariantCultureIgnoreCase);
            if (posEnd > -1)
            {
                string macro = str.Substring(posStart, posEnd - posStart);

                // found a macro
                macros.Add(macro);

                // skip the macro end before searching for the next macroBeg
                posEnd += macroEnd.Length;

                posStart = str.IndexOf(macroBeg, posEnd, StringComparison.InvariantCultureIgnoreCase);
            }
            else
            {
                break;
            }
        }

        return macros != null ? macros.ToArray() : new string[0];
    }
}