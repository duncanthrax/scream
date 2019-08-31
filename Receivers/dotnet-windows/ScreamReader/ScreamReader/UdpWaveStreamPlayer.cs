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
        
        private Semaphore startLock;

        private Semaphore shutdownLock;

        private CancellationTokenSource cancellationTokenSource;

        private UdpClient udpClient;

        private WasapiOut output;

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
                if (this.output != null)
                {
                    this.output.Volume = (float)value / 100f;
                }
            }
        }
        #endregion


        /// <summary>
        /// Default c'tor that supports Scream's default settings.
        /// </summary>
        public UdpWaveStreamPlayer()
            : this(ScreamMulticastAddress, ScreamMulticastPort)
        {
        }

        /// <summary>
        /// Initialize the client with the specific address, port and format.
        /// </summary>
        /// <param name="multicastAddress"></param>
        /// <param name="multicastPort"></param>
        public UdpWaveStreamPlayer(IPAddress multicastAddress, int multicastPort)
        {
            this.multicastAddress = multicastAddress;
            this.multicastPort = multicastPort;

            this.startLock = new Semaphore(1, 1);
            this.shutdownLock = new Semaphore(0, 1);
            
            this.udpClient = new UdpClient
            {
                ExclusiveAddressUse = false
            };
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
                var currentRate = 129;
                var currentWidth = 16;
                var currentChannels = 2;
                var currentChannelsMapLsb = 0x03; // stereo
                var currentChannelsMapMsb = 0x00;
                var currentChannelsMap = (currentChannelsMapMsb << 8) | currentChannelsMapLsb;
                var localEp = new IPEndPoint(IPAddress.Any, this.multicastPort);

                this.udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                this.udpClient.Client.Bind(localEp);
                this.udpClient.JoinMulticastGroup(this.multicastAddress);

                var rsws = new BufferedWaveProvider(new WaveFormat(44100, currentWidth, currentChannels)) { BufferDuration = TimeSpan.FromMilliseconds(200), DiscardOnBufferOverflow = true };

                this.output = new WasapiOut();
                this.Volume = 100;

                this.output.Init(rsws);
                this.output.Play();

                Task.Factory.StartNew(() =>
                {
                    while (!this.cancellationTokenSource.IsCancellationRequested)
                    {
                        try
                        {
                            Byte[] data = this.udpClient.Receive(ref localEp);
                            
                            if (data[0] != currentRate || data[1] != currentWidth || data[2] != currentChannels || data[3] != currentChannelsMapLsb || data[4] != currentChannelsMapMsb)
                            {
                                currentRate = data[0];
                                currentWidth = data[1];
                                currentChannels = data[2];
                                currentChannelsMapLsb = data[3];
                                currentChannelsMapMsb = data[4];
                                currentChannelsMap = (currentChannelsMapMsb << 8) | currentChannelsMapLsb;

                                // TODO find a way to set a channel map in NAudio. I was not able to find any.
                                // In practice if both the source and the receiver windows machine have the same speakers configuration setted this doesn't matter,
                                // but in all other cases the channels will be possibly mismatched.
                                this.output.Stop();
                                var rate = ((currentRate >= 128) ? 44100 : 48000) * (currentRate % 128);

                                rsws = new BufferedWaveProvider(new WaveFormat(rate, currentWidth, currentChannels)) { BufferDuration = TimeSpan.FromMilliseconds(200), DiscardOnBufferOverflow = true };
                                this.output = new WasapiOut();
                                this.Volume = 100;
                                this.output.Init(rsws);
                                this.output.Play();
                            }
                            rsws.AddSamples(data, 5, data.Length - 5);
                        } catch (SocketException) { } // Usually when interrupted
                        catch(Exception e)
                        {
                            System.Windows.Forms.MessageBox.Show(e.StackTrace, e.Message);
                           
                        }
                    }
                }, this.cancellationTokenSource.Token);

                this.shutdownLock.WaitOne();

                this.output.Stop();
                this.udpClient.Close();
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
                this.startLock.Dispose();
                this.shutdownLock.Dispose();
            }
        }
        #endregion
    }
}
