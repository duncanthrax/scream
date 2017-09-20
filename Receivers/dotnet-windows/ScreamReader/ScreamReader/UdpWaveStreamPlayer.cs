using NAudio.Wave;
using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace ScreamReader
{
    internal class UdpWaveStreamPlayer : IDisposable
    {
        #region static defaults
        /// <summary>
        /// The <see cref="IPAddress"/> scream is broadcasting to.
        /// </summary>
        public static readonly IPAddress ScreamMulticastAddress =
             IPAddress.Parse("239.255.77.77");

        /// <summary>
        /// The port scream is broadcasting on.
        /// </summary>
        public static readonly int ScreamMulticastPort = 4010;

        /// <summary>
        /// Default <see cref="waveFormat"/> is 44.1 kHz, 16 bits, stereo.
        /// </summary>
        public static readonly WaveFormat ScreamWaveFormat = new WaveFormat();
        #endregion

        #region instance variables
        /// <summary>
        /// The <see cref="IPAddress"/> in use.
        /// </summary>
        protected IPAddress multicastAddress { get; set; }

        /// <summary>
        /// The port to listen to.
        /// </summary>
        protected int multicastPort { get; set; }

        /// <summary>
        /// The used <see cref="waveFormat"/>.
        /// </summary>
        protected WaveFormat waveFormat { get; set; }

        private Semaphore startLock;

        private Semaphore shutdownLock;

        private CancellationTokenSource cancellationTokenSource;

        private UdpClient udpClient;

        private WaveOut output;

        private int volume;
        #endregion

        #region public properties
        /// <summary>
        /// Used to control the volume. Valid values are [0, 100].
        /// </summary>
        public int Volume
        {
            get { return this.volume; }
            set
            {
                if (value < 0 || value > 100)
                {
                    throw new ArgumentOutOfRangeException(nameof(value));
                }

                this.volume = value;
                this.output.Volume = (float)value / 100f;
            }
        }
        #endregion


        /// <summary>
        /// Default c'tor that supports Scream's default settings.
        /// </summary>
        public UdpWaveStreamPlayer()
            : this(ScreamMulticastAddress, ScreamMulticastPort, ScreamWaveFormat)
        {
        }

        /// <summary>
        /// Initialize the client with the specific address, port and format.
        /// </summary>
        /// <param name="multicastAddress"></param>
        /// <param name="multicastPort"></param>
        /// <param name="waveFormat"></param>
        public UdpWaveStreamPlayer(IPAddress multicastAddress, int multicastPort, WaveFormat waveFormat)
        {
            this.multicastAddress = multicastAddress;
            this.multicastPort = multicastPort;
            this.waveFormat = waveFormat;

            this.startLock = new Semaphore(1, 1);
            this.shutdownLock = new Semaphore(0, 1);
            this.output = new WaveOut()
            {
                DesiredLatency = 80
            };

            this.udpClient = new UdpClient
            {
                ExclusiveAddressUse = false
            };

            this.Volume = 100;
        }

        /// <summary>
        /// Starts listening to the broadcast and plays back audio received from it.
        /// Subsequent calls to this method require to call <see cref="Stop"/> in between.
        /// </summary>
        public virtual void Start()
        {
            this.startLock.WaitOne();
            this.cancellationTokenSource = new CancellationTokenSource();

            Task.Factory.StartNew(() =>
            {
                var localEp = new IPEndPoint(IPAddress.Any, this.multicastPort);

                this.udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                this.udpClient.Client.Bind(localEp);
                this.udpClient.JoinMulticastGroup(this.multicastAddress);


                var rsws = new BufferedWaveProvider(new WaveFormat())
                {
                    BufferDuration = TimeSpan.FromMilliseconds(500)
                };

                this.output.Init(rsws);
                this.output.Play();

                Task.Factory.StartNew(() =>
                {
                    while (!this.cancellationTokenSource.IsCancellationRequested)
                    {
                        try
                        {
                            Byte[] data = this.udpClient.Receive(ref localEp);
                            rsws.AddSamples(data, 0, data.Length);
                        } catch (SocketException) { } // Usually when interrupted
                    }
                }, this.cancellationTokenSource.Token);

                this.shutdownLock.WaitOne();

                this.udpClient.Close();
                this.output.Stop();
            }, this.cancellationTokenSource.Token);
        }

        /// <summary>
        /// Stops reading data from the broadcast and playing it back.
        /// </summary>
        public void Stop()
        {
            this.shutdownLock.Release();
            this.cancellationTokenSource.Cancel();
            this.startLock.Release();
        }

        #region dispose
        public void Dispose()
        {
            this.Dispose(true);
        }

        public virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                this.udpClient.Dispose();
                this.startLock.Dispose();
                this.shutdownLock.Dispose();
            }
        }
        #endregion
    }
}
