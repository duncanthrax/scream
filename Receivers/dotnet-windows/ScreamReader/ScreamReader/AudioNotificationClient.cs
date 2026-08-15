using NAudio.CoreAudioApi;
using NAudio.CoreAudioApi.Interfaces;
using System;

namespace ScreamReader
{
    internal class AudioNotificationClient : IMMNotificationClient
    {
        public class DefaultDeviceChangedEventArgs : EventArgs
        {
            public DataFlow Flow { get; }
            public Role Role { get; }
            public string DeviceId { get; }

            public DefaultDeviceChangedEventArgs(DataFlow flow, Role role, string deviceId)
            {
                Flow = flow;
                Role = role;
                DeviceId = deviceId;
            }
        }

        public EventHandler<DefaultDeviceChangedEventArgs> DefaultDeviceChanged;

        public void OnDeviceStateChanged(string deviceId, DeviceState newState) { }
        public void OnDeviceAdded(string pwstrDeviceId) { }
        public void OnDeviceRemoved(string deviceId) { }

        public void OnDefaultDeviceChanged(DataFlow flow, Role role, string defaultDeviceId)
        {
            DefaultDeviceChanged?.Invoke(this, new DefaultDeviceChangedEventArgs(flow, role, defaultDeviceId));
        }

        public void OnPropertyValueChanged(string pwstrDeviceId, PropertyKey key) { }
    }
}
