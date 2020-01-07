using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using TMPro;

public class GameAnalyticsEntity : MonoBehaviour
{
    public Analytics.GameEntityInfo CachedInfo { get; protected set; }

    GameObject _primitive = null;
    TextMeshPro _textMeshPro = null;

    GameObject _eye = null;

    public GameAnalyticsEntity()
    {
        CachedInfo = new Analytics.GameEntityInfo();
    }

    public void UpdateData(Analytics.GameEntityInfo info)
    {
        CachedInfo = info;
        
        transform.eulerAngles = new Vector3(info.EulerRotation.X, info.EulerRotation.Y, info.EulerRotation.Z);
        transform.position = new Vector3(info.Position.X, info.Position.Y, info.Position.Z);

        if(info.EyeOffset != null || info.EyeDir != null)
        {
            if(_eye == null)
            {
                _eye = new GameObject("eye");
                _eye.transform.parent = transform;
                _eye.transform.localPosition = Vector3.zero;
                _eye.transform.localRotation = Quaternion.identity;
                _eye.transform.localScale = Vector3.one;
            }

            if(info.EyeOffset != null)
                _eye.transform.localPosition = new Vector3(info.EyeOffset.X, info.EyeOffset.Z, info.EyeOffset.Y);
            if(info.EyeDir != null)
            {
                _eye.transform.rotation = Quaternion.LookRotation(new Vector3(info.EyeDir.X, info.EyeDir.Z, info.EyeDir.Y), transform.up);
            }
        }
        
        if (_primitive == null)
        {
            _primitive = GameObject.CreatePrimitive(PrimitiveType.Cube);
            _primitive.transform.parent = transform;
            _primitive.transform.localRotation = Quaternion.identity;
        }

        // update bounds representation

        // swizzle for coordinate system
        if(CachedInfo.BoundsMin != null)
        {
            Vector3 mins = new Vector3(CachedInfo.BoundsMin.X, CachedInfo.BoundsMin.Y, CachedInfo.BoundsMin.Z);
            Vector3 maxs = new Vector3(CachedInfo.BoundsMax.X, CachedInfo.BoundsMax.Y, CachedInfo.BoundsMax.Z);
            Vector3 extents = maxs - mins;

            _primitive.transform.localPosition = (maxs + mins) * 0.5f;
            _primitive.transform.localScale = new Vector3(extents.x, extents.y, extents.z);

            //if(_textMeshPro == null)
            //{
            //    _textMeshPro = new GameObject("Name", typeof(TextMeshPro)).GetComponent<TextMeshPro>();
            //    _textMeshPro.color = Color.blue;
            //    _textMeshPro.transform.SetParent(transform, false);
            //    _textMeshPro.transform.localPosition = Vector3.zero;
            //    _textMeshPro.transform.localRotation = Quaternion.identity;
            //    _textMeshPro.transform.localScale = Vector3.one;
            //    _textMeshPro.alignment = TextAlignmentOptions.Midline;

            //    RectTransform rt = _textMeshPro.GetComponent<RectTransform>();
            //    rt.SetSizeWithCurrentAnchors(RectTransform.Axis.Horizontal, 80.0f);

            //    _textMeshPro.text = gameObject.name;
            //}
            if(_textMeshPro != null)
                _textMeshPro.transform.localPosition = _primitive.transform.localPosition + Vector3.up * (extents.y * 0.5f + 5.0f);
        }
        else
        {
            _primitive.transform.localPosition = Vector3.zero;
            _primitive.transform.localScale = Vector3.one * 0.1f;
        }
    }
}
