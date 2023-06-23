use std::cmp;
use std::cmp::min;
use std::collections::btree_map::Range;
use std::convert::TryInto;
use std::result;
use std::time;
use std::time::Duration;
use std::time::Instant;
use std::net::SocketAddr;

use std::str::FromStr;

use std::collections::HashSet;
use std::collections::VecDeque;
use std::collections::hash_map;
use std::collections::BTreeMap;
use std::collections::BinaryHeap;
use std::collections::HashMap;
use std::vec;
use rand::Rng;

const HEADER_LENGTH: usize = 26;


// use crate::ranges;

/// The minimum length of Initial packets sent by a client.
pub const MIN_CLIENT_INITIAL_LEN: usize = 1350;

#[cfg(not(feature = "fuzzing"))]
const PAYLOAD_MIN_LEN: usize = 4;



// The default max_datagram_size used in congestion control.
const MAX_SEND_UDP_PAYLOAD_SIZE: usize = 1350;

// The default length of DATAGRAM queues.
const DEFAULT_MAX_DGRAM_QUEUE_LEN: usize = 0;


const SEND_BUFFER_SIZE:usize = 1024;

pub type Result<T> = std::result::Result<T, Error>;

/// A QUIC error.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    /// There is no more work to do.
    Done,

    /// The provided buffer is too short.
    BufferTooShort,

    /// The provided packet cannot be parsed because its version is unknown.
    UnknownVersion,

    /// The provided packet cannot be parsed because it contains an invalid
    /// frame.
    InvalidFrame,

    /// The provided packet cannot be parsed.
    InvalidPacket,

    /// The operation cannot be completed because the connection is in an
    /// invalid state.
    InvalidState,

    /// The operation cannot be completed because the stream is in an
    /// invalid state.
    ///
    /// The stream ID is provided as associated data.
    InvalidStreamState(u64),

    /// The peer's transport params cannot be parsed.
    InvalidTransportParam,

    /// A cryptographic operation failed.
    CryptoFail,

    /// The TLS handshake failed.
    TlsFail,

    /// The peer violated the local flow control limits.
    FlowControl,

    /// The peer violated the local stream limits.
    StreamLimit,

    /// The specified stream was stopped by the peer.
    ///
    /// The error code sent as part of the `STOP_SENDING` frame is provided as
    /// associated data.
    StreamStopped(u64),

    /// The specified stream was reset by the peer.
    ///
    /// The error code sent as part of the `RESET_STREAM` frame is provided as
    /// associated data.
    StreamReset(u64),

    /// The received data exceeds the stream's final size.
    FinalSize,

    /// Error in congestion control.
    CongestionControl,

    /// Too many identifiers were provided.
    IdLimit,

    /// Not enough available identifiers.
    OutOfIdentifiers,
}

impl Error {
    fn to_wire(self) -> u64 {
        match self {
            Error::Done => 0x0,
            Error::InvalidFrame => 0x7,
            Error::InvalidStreamState(..) => 0x5,
            Error::InvalidTransportParam => 0x8,
            Error::FlowControl => 0x3,
            Error::StreamLimit => 0x4,
            Error::FinalSize => 0x6,
            _ => 0xa,
        }
    }

    #[cfg(feature = "ffi")]
    fn to_c(self) -> libc::ssize_t {
        match self {
            Error::Done => -1,
            Error::BufferTooShort => -2,
            Error::UnknownVersion => -3,
            Error::InvalidFrame => -4,
            Error::InvalidPacket => -5,
            Error::InvalidState => -6,
            Error::InvalidStreamState(_) => -7,
            Error::InvalidTransportParam => -8,
            Error::CryptoFail => -9,
            Error::TlsFail => -10,
            Error::FlowControl => -11,
            Error::StreamLimit => -12,
            Error::FinalSize => -13,
            Error::CongestionControl => -14,
            Error::StreamStopped { .. } => -15,
            Error::StreamReset { .. } => -16,
            Error::IdLimit => -17,
            Error::OutOfIdentifiers => -18,
        }
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl std::error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        None
    }
}

impl std::convert::From<octets::BufferTooShortError> for Error {
    fn from(_err: octets::BufferTooShortError) -> Self {
        Error::BufferTooShort
    }
}

/// Ancillary information about incoming packets.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RecvInfo {
    /// The remote address the packet was received from.
    pub from: SocketAddr,

    /// The local address the packet was received on.
    pub to: SocketAddr,
}

/// Ancillary information about outgoing packets.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SendInfo {
    /// The local address the packet should be sent from.
    pub from: SocketAddr,

    /// The remote address the packet should be sent to.
    pub to: SocketAddr,

}

/// Stores configuration shared between multiple connections.
pub struct Config {

    cc_algorithm: CongestionControlAlgorithm,

    max_send_udp_payload_size: usize,

    max_idle_timeout: u64,
}

impl Config {
    /// Creates a config object with the given version.
    ///
    /// ## Examples:
    ///
    /// ```
    /// let config = quiche::Config::new()?;
    /// # Ok::<(), quiche::Error>(())
    /// ```
    pub fn new() -> Result<Config> {
        Ok(Config {
            // local_transport_params: TransportParams::default(),
            cc_algorithm: CongestionControlAlgorithm::NEWCUBIC,
            // pacing: true,

            max_send_udp_payload_size: MAX_SEND_UDP_PAYLOAD_SIZE,

            max_idle_timeout: 5000,
        })
    }

    /// Sets the `max_idle_timeout` transport parameter, in milliseconds.
    /// same with tcp max idle timeout
    /// The default value is infinite, that is, no timeout is used.
    pub fn set_max_idle_timeout(&mut self, v: u64) {
        self.max_idle_timeout = v;
    }
   

    /// Sets the congestion control algorithm used by string.
    ///
    /// The default value is `cubic`. On error `Error::CongestionControl`
    /// will be returned.
    ///
    /// ## Examples:
    ///
    /// ```
    // # let mut config = quiche::Config::new(0xbabababa)?;
    /// config.set_cc_algorithm_name("NEWCUBIC");
    /// # Ok::<(), quiche::Error>(())
    /// ```
    pub fn set_cc_algorithm_name(&mut self, name: &str) -> Result<()> {
        self.cc_algorithm = CongestionControlAlgorithm::from_str(name)?;

        Ok(())
    }

    /// Sets the congestion control algorithm used
    ///
    /// The default value is `CongestionControlAlgorithm::CUBIC`.
    pub fn set_cc_algorithm(&mut self, algo: CongestionControlAlgorithm) {
        self.cc_algorithm = algo;
    }

}

#[inline]
pub fn accept(
    local: SocketAddr,
    peer: SocketAddr, config: &mut Config,
) -> Result<Connection> {
    let conn = Connection::new(local, peer, config, true)?;

    Ok(conn)
}


#[inline]
pub fn connect(
    local: SocketAddr,
    peer: SocketAddr, config: &mut Config,
) -> Result<Connection> {
    let conn = Connection::new(local, peer, config, false)?;

    Ok(conn)
}


pub struct Connection {

    /// Total number of received packets.
    recv_count: usize,

    /// Total number of sent packets.
    sent_count: usize,

    /// Total number of lost packets.
    lost_count: usize,

    /// Total number of packets sent with data retransmitted.
    retrans_count: usize,


    /// Total number of bytes sent lost over the connection.
    lost_bytes: u64,

    /// Draining timeout expiration time.
    draining_timer: Option<time::Instant>,

    /// Whether this is a server-side connection.
    is_server: bool,

    /// Whether the connection handshake has been completed.
    handshake_completed: bool,

    /// Whether the HANDSHAKE_DONE frame has been sent.
    handshake_done_sent: bool,

    /// Whether the HANDSHAKE_DONE frame has been acked.
    handshake_done_acked: bool,

    /// Whether the connection handshake has been confirmed.
    handshake_confirmed: bool,

    /// Whether an ack-eliciting packet has been sent since last receiving a
    /// packet.
    ack_eliciting_sent: bool,

    /// Whether the connection is closed.
    closed: bool,

    // Whether the connection was timed out
    timed_out: bool,

    #[cfg(feature = "qlog")]
    qlog: QlogInfo,

    server: bool,

    localaddr: SocketAddr,

    peeraddr: SocketAddr,

    recovery: Recovery,

    pkt_num_spaces: [packet::PktNumSpace; 2],

    rtt: time::Duration,
    
    handshake: time::Instant,

    send_buffer: SendBuf,

    rec_buffer: RecvBuf,

    written_data: usize,

    stop_flag: bool,

    prioritydic: HashMap<u64,u8>,

    off: u64,

    sent_pkt:Vec<u64>,

    recv_pkt:Vec<u64>,

    recv_dic: HashMap<u64,u8>,

    //used to compute priority
    low_split_point: f32,
    high_split_point: f32,

    //store data
    send_data:Vec<u8>,
    //store norm2 for every 256 bits float
    norm2_vec:Vec<f32>,

    //total offset for the each iteration parameter
    offset_vec:Vec<u64>,

    total_offset:u64,

    recv_flag: bool,

    recv_hashmap: HashMap<u64, u64>,

}

impl Connection {
    /// Sets the congestion control algorithm used by string.
    /// Examples:
    /// let mut config = quiche::Config::new()?;
    /// let local = "127.0.0.1:0".parse().unwrap();
    /// let peer = "127.0.0.1:1234".parse().unwrap();
    /// let conn = quiche::new(local, peer, &mut config, true)?
    /// # Ok::<(), quiche::Error>(())
    /// ```
    fn new(
        local: SocketAddr,
        peer: SocketAddr, config: &mut Config, is_server: bool,
    ) ->  Result<Connection> {


        let mut conn = Connection {

            pkt_num_spaces: [
                packet::PktNumSpace::new(),
                packet::PktNumSpace::new(),
            ],


            recv_count: 0,
            sent_count: 0,
            lost_count: 0,
            retrans_count: 0,

            lost_bytes: 0,


            draining_timer: None,
            is_server,

            // Assume clients validate the server's address implicitly.
            handshake_completed: false,

            handshake_done_sent: false,
            handshake_done_acked: false,

            handshake_confirmed: true,

            ack_eliciting_sent: false,

            closed: false,

            timed_out: false,

            server: is_server,

            localaddr: local,

            peeraddr: peer,

            recovery: recovery::Recovery::new(&config),

            rtt: Duration::ZERO,
            
            handshake: Instant::now(),
            
            send_buffer: SendBuf::new((MIN_CLIENT_INITIAL_LEN*8).try_into().unwrap()),

            rec_buffer: RecvBuf::new(),
            
            written_data:0,

            //All buffer data have been sent, waiting for the ack responce.
            stop_flag: false,

            prioritydic:HashMap::new(),

            recv_dic:HashMap::new(),

            off: 0,

            sent_pkt:Vec::<u64>::new(),
            recv_pkt:Vec::<u64>::new(),
            
            low_split_point:0.0,
            high_split_point:0.0,

            send_data:Vec::<u8>::new(),
            norm2_vec:Vec::<f32>::new(),

            offset_vec:Vec::<u64>::new(),
            total_offset:0,

            recv_flag: false,

            recv_hashmap:HashMap::new(),
        };

        conn.recovery.on_init();
 

        Ok(conn)
    }

    fn update_rtt(&mut self){
        let arrive_time = Instant::now();
        self.rtt = arrive_time.duration_since(self.handshake);
    }

    pub fn recv_slice(&mut self, buf: &mut [u8]) ->Result<usize>{
        let len = buf.len();

        if len == 0{
            return Err(Error::BufferTooShort);
        }
        self.recv_count += 1;

        let mut b = octets::OctetsMut::with_slice(buf);

        let hdr = Header::from_bytes(&mut b)?;

        let mut read:usize = 0;

        if hdr.ty == packet::Type::Handshake && self.is_server{
            self.update_rtt();
            self.handshake_completed = true;
        }
        
        if hdr.ty == packet::Type::Handshake && !self.is_server{
            self.handshake_confirmed = false;
        }
        //receiver send back the sent info to sender
        if hdr.ty == packet::Type::ACK && self.is_server{
            self.process_ack(buf);
        }

        if hdr.ty == packet::Type::ElictAck{
            self.check_loss(&mut buf[26..]);
        }

        if hdr.ty == packet::Type::Application{
            self.recv_count += 1;
            read = hdr.pkt_length as usize;
            self.rec_buffer.write(&mut buf[26..],hdr.offset).unwrap();
            self.prioritydic.insert(hdr.offset, hdr.priority);
            self.recv_dic.insert(hdr.offset, hdr.priority);
            if self.pkt_num_spaces[0].recv_pkt_num.get_priority(hdr.offset) == 0{
                self.pkt_num_spaces[0].recv_pkt_num.additem(hdr.offset, hdr.priority as usize);
            }else {
                self.pkt_num_spaces[0].recv_pkt_num.update_item(hdr.offset, hdr.priority as usize);
            }
        }

        Ok(read)
    }

    

    //Get unack offset. 
    fn process_ack(&mut self, buf: &mut [u8]){
        let unackbuf = &buf[27..];
        let len = unackbuf.len();
        let mut start = 0;
        let mut weights:f32 = 0.0;
        // let mut b = octets::OctetsMut::with_slice(buf);
        let mut ack_set:u64 = 0 ;
        while start <= len{
            let unack = u64::from_be_bytes(unackbuf[start..start+8].try_into().unwrap());
            start += 8;
            let priority = u64::from_be_bytes(unackbuf[start..start+8].try_into().unwrap());
            start += 8;
            if priority == 1{
                weights += 0.1;
            }else if priority == 2 {
                weights += 0.2;
            }else if priority == 3 {
                weights += 0.45;
            }else{
                self.send_buffer.ack_and_drop(unack);
            }

            
        }

        ///////////////////////////////////////////////////////////////////////
        // update congestion window size
        /////////////////////////////////////////////////////////////
        // if len > 0{
        //     let mut flag:bool = true;
        //     self.recovery.update_app_limited(b);
        // }
        // else{
        //     let mut flag:bool = false;
        //     self.recovery.update_app_limited(b);
        // }
        // self.recovery.update_app_window(weights);
        self.recovery.update_win(weights, (len/8) as f64);
        
    }

    pub fn findweight(&mut self, unack:&u64)->u8{
        *self.prioritydic.get(unack).unwrap()
    }

    //pub fn send_all(&mut self, data: &mut [u8]) -> Result<bool> {
    pub fn send_all(&mut self) -> Result<bool> {
        self.stop_flag = false;

        //let self.position = self.get_position();
        // let write_to_buffer = &data[self.written_data..];
        
        // let write = self.write(write_to_buffer, self.recovery.cwnd());

        //let write = self.write(data);
        
        // send_all == true
        // while true
        // send()
        // otherwise,
        // break
        // Add new flag to ensure there are data to send
        // check send_buff == 0
        // 1. no new data written into the buffer, but still has new data
        // 2. no new data written into the buffer, no new data.
        //
        // if send_all()
        // if send_all() == false
        // check self.send_data.len()
        // no longer send_all()
        if self.send_buffer.recv_index.len() != 0{
            self.send_buffer.recv_and_drop();
        }


        if self.send_data.len() > self.written_data{
            let write = self.write();
            self.written_data += write.unwrap();
            Ok(true)
        }else {
            Ok(false)
        }
        
    }

    pub fn send_data(&mut self, out: &mut [u8])-> Result<(usize, SendInfo)>{
        if out.is_empty(){
            return Err(Error::BufferTooShort);
        }
        
        let mut done = 0;
        let mut total_len:usize = HEADER_LENGTH;

        // Limit output packet size to respect the sender and receiver's
        // maximum UDP payload size limit.
        let mut left = cmp::min(out.len(), self.max_send_udp_payload_size());

        let mut pn:u64 = 0;
        let mut offset:u64 = 0;
        let mut priority:u8 = 0;
        let mut psize:u64 = 0;


        let ty = self.write_pkt_type()?; 

        // if ty == packet::Type::Handshake && self.server{
        if ty == packet::Type::Handshake{
            let hdr = Header {
                ty,
                pkt_num: pn,
                offset: offset,
                priority: priority,
                pkt_length: psize,
            };
            let mut b = octets::OctetsMut::with_slice(out);
            hdr.to_bytes(&mut b)?;
        }
        
        //send the received packet condtion
        if ty == packet::Type::ACK{
            let mut b = octets::OctetsMut::with_slice(out);
            let hdr = Header {
                ty,
                pkt_num: 0,
                offset: 0,
                priority: 0,
                pkt_length: 8*16,
            };
            hdr.to_bytes(&mut b)?;

            /// pkt_length may not be 8
            for (key, val) in self.recv_hashmap.iter() {
                b.put_u64(*key)?;
                b.put_u64(*val)?;
            }

            self.recv_hashmap.clear();
        }

        //send the 
        if ty == packet::Type::ElictAck{
            let mut b = octets::OctetsMut::with_slice(out);
            if self.stop_flag == true{
                //When send_buf send out all data
                let pkt_counter = self.sent_pkt.len() - self.sent_pkt.len()%8;
                let res = &self.sent_pkt[pkt_counter..];
                let hdr = Header{
                    ty,
                    pkt_num: pn,
                    offset: offset,
                    priority: priority,
                    pkt_length: (pkt_counter*8) as u64,
                };
                hdr.to_bytes(&mut b).unwrap();
                for i in 0..res.len() as usize{
                    b.put_u64(res[i])?;
                }
            }
            else{
                //normally, every 8 pakcets will send a ElictAck packet.
                let res = &self.sent_pkt[(self.sent_pkt.len()-self.sent_pkt.len()%8)..];
                let hdr = Header{
                    ty,
                    pkt_num: pn,
                    offset: offset,
                    priority: priority,
                    pkt_length: 64,
                };
                hdr.to_bytes(&mut b).unwrap();
                for i in 0..res.len() as usize{
                    b.put_u64(res[i])?;
                }
            }
            
        }
        

        if ty == packet::Type::Application{
            if let Ok((result_len, off, stop)) = self.send_buffer.emit(&mut out[26..]){

                let mut b = octets::OctetsMut::with_slice(&mut out[done..]);
                if ty == packet::Type::Application{
                    pn = self.pkt_num_spaces[0].next_pkt_num;
                    
                    priority = self.priority_calculation(off);
                    self.pkt_num_spaces[0].next_pkt_num += 1;
                }
                let hdr = Header {
                    ty,
                    pkt_num: pn,
                    offset: off,
                    priority: priority,
                    pkt_length: result_len as u64,
                };
    
                hdr.to_bytes(&mut b)?;

                //Recording offset of each data.
                // self.total_offset = std::cmp::max(off, self.total_offset);
    
                if stop == true{
                    self.stop_flag = true;
                }
                self.sent_pkt.push(hdr.offset);
            
            }
        }
        

        let info = SendInfo {
            from: self.localaddr,
            to: self.peeraddr,
        };
        self.handshake = time::Instant::now();


        Ok((total_len, info))
    }



    pub fn read(&self, length:usize){

    }
  
    //Writing data to send buffer.
    pub fn write(&mut self) -> Result<usize> {
        //?/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        let off_len = 1024 - (self.total_offset % 1024) as usize;
        //Note: written_data refers to the non-retransmitted data.
        self.send_buffer.write(&self.send_data[self.written_data..], self.recovery.cwnd(), off_len)
    }

    pub fn  priority_calculation(&self, off: u64) -> u8{
        let real_index = off/1024;
        if self.norm2_vec[real_index as usize] < self.low_split_point{
            1
        }
        else if self.norm2_vec[real_index as usize] < self.high_split_point {
            2
        }
        else {
            3
        }

    }

    pub fn reset(& mut self){
        self.norm2_vec.clear();
        self.send_buffer.clear();
    }

    ///responce packet used to tell sender which packet loss
    // fn send_ack(&mut self, recv_buf: &mut [u8],out:&mut [u8]) -> Header{
    //     let mut res:Vec<u64> = Vec::new(); 
    //     res = self.check_loss(recv_buf);  
    //     let pn:u64 = 0;
    //     let offset:u64 = 0;
    //     let priority:u8 = 0;
    //     let plen:u64 = res.len().try_into().unwrap();
    //     let psize:u64 = plen*4;
    //     let ty = packet::Type::ACK;
    //     let mut b = octets::OctetsMut::with_slice(out);
    //     //let pkt_type = packet::Type::ElictAck;
    //     let hdr= Header{
    //         ty: ty,
    //         pkt_num: pn,
    //         offset: offset,
    //         priority: priority,
    //         pkt_length: psize,
    //     };
    //     hdr.to_bytes(&mut b).unwrap();
    //     for i in 0..plen as usize{
    //         b.put_u64(res[i]).unwrap();
    //     } 
    //     hdr
    // }

    fn record_reset(&mut self){
    
    }

    // fn check_loss(&mut self, recv_buf: &mut [u8])->Vec<u64>{
    //     let mut offset:u64 = 0;
    //     let mut b = octets::OctetsMut::with_slice(recv_buf);
    //     let mut result:Vec<u64> = Vec::new();
    //     while b.cap()>0 {
    //         offset = b.get_u64().unwrap();
    //         if !self.pkt_num_spaces[0].recv_pkt_num.record.contains_key(&offset){
    //             self.pkt_num_spaces[0].recv_pkt_num.additem(offset,3,1350);
    //             result.push(offset);
    //         }
    //         else{
    //             //u64 to usize try_into().unwrap()
    //             //let rtime:usize = self.pkt_num_spaces[0].recv_pkt_num.record[&offset][2].try_into().unwrap();
    //             let rtime:u64 = self.pkt_num_spaces[0].recv_pkt_num.record[&offset][2];

    //             let recived:u64 = self.pkt_num_spaces[0].recv_pkt_num.record[&offset][3];
    //             if (recived == 0) & (rtime >0){
    //                 result.push(offset);
    //             }
    //         }
    //     }
    //     result
    // }

    pub fn check_loss(&mut self, recv_buf: &mut [u8]){
        let mut offset:u64 = 0;
        let mut b = octets::OctetsMut::with_slice(recv_buf);
        let result:Vec<u64> = Vec::new();
        while b.cap()>0 {
            offset = b.get_u64().unwrap();
            // priority == 0 means that packet received
            if self.pkt_num_spaces[0].recv_pkt_num.check_received(offset){
                self.recv_hashmap.insert(offset, 0);
            }else {
                self.recv_hashmap.insert(offset, self.pkt_num_spaces[0].recv_pkt_num.get_retrans_times(offset));
            }
        }

    }

    pub fn set_handshake(&mut self){
        self.handshake = Instant::now();
    }

/////////////////////////////////////////////////////
    /// get offset and priority pair function
    /// pub fn get_record()->hashmap(u64,u8){
    /// }
/////////////////////////////////////////////////
    ///pub ty: Type,
    // pub(crate) pkt_num: u64,
    // pub priority:u8,
    // pub(crate) offset:u64,
    // pub(crate) pkt_length:u64,

    pub fn retry(&mut self,out: &mut [u8], pri: u8, offset:u64,
    ) -> Result<usize> {
        let mut b = octets::OctetsMut::with_slice(out);

        let hdr = Header {
            ty: Type::Retry,
            pkt_num: self.pkt_num_spaces[0].next_pkt_num,
            priority: pri,
            offset: offset,
            pkt_length: self.getretrylength(offset)?,
        };
    
        self.pkt_num_spaces[0].next_pkt_num +=1;

        hdr.to_bytes(&mut b)?;
    
        Ok(b.off())
    }

    pub fn getretrylength(&mut self,offset:u64) -> Result<u64>{
        let length = 1024;
        Ok(length)
    }

    /// Returns the maximum possible size of egress UDP payloads.
    ///
    /// This is the maximum size of UDP payloads that can be sent, and depends
    /// on both the configured maximum send payload size of the local endpoint
    /// (as configured with [`set_max_send_udp_payload_size()`]), as well as
    /// the transport parameter advertised by the remote peer.
    ///
    /// Note that this value can change during the lifetime of the connection,
    /// but should remain stable across consecutive calls to [`send()`].
    ///
    /// [`set_max_send_udp_payload_size()`]:
    ///     struct.Config.html#method.set_max_send_udp_payload_size
    /// [`send()`]: struct.Connection.html#method.send
    pub fn max_send_udp_payload_size(&self) -> usize {
        MIN_CLIENT_INITIAL_LEN
    }
    

    /// Returns true if the connection handshake is complete.
    #[inline]
    pub fn is_established(&self) -> bool {
        self.handshake_completed
    }


    /// Returns true if the connection is draining.
    ///
    /// If this returns `true`, the connection object cannot yet be dropped, but
    /// no new application data can be sent or received. An application should
    /// continue calling the [`recv()`], [`timeout()`], and [`on_timeout()`]
    /// methods as normal, until the [`is_closed()`] method returns `true`.
    ///
    /// In contrast, once `is_draining()` returns `true`, calling [`send()`]
    /// is not required because no new outgoing packets will be generated.
    ///
    /// [`recv()`]: struct.Connection.html#method.recv
    /// [`send()`]: struct.Connection.html#method.send
    /// [`timeout()`]: struct.Connection.html#method.timeout
    /// [`on_timeout()`]: struct.Connection.html#method.on_timeout
    /// [`is_closed()`]: struct.Connection.html#method.is_closed
    #[inline]
    pub fn is_draining(&self) -> bool {
        self.draining_timer.is_some()
    }

    /// Returns true if the connection is closed.
    ///
    /// If this returns true, the connection object can be dropped.
    #[inline]
    pub fn is_closed(&self) -> bool {
        self.closed
    }

    /// Returns true if the connection was closed due to the idle timeout.
    #[inline]
    pub fn is_timed_out(&self) -> bool {
        self.timed_out
    }

    
    /// Selects the packet type for the next outgoing packet.
    fn write_pkt_type(& mut self) -> Result<packet::Type> {
        // let now = Instant::now();
        if self.rtt == Duration::ZERO && self.is_server == true{
            return Ok(packet::Type::Handshake);
        }

        if self.handshake_confirmed == false && self.is_server == false{
            self.handshake_confirmed = true;
            return Ok(packet::Type::Handshake);
        }

        if (self.sent_count % 8 == 0 && self.sent_count > 0) || self.stop_flag == true{
            return Ok(packet::Type::ElictAck);
        }

        // if self.recv_count % 8 == 0 && self.recv_count > 0 {
        //     return Ok(packet::Type::ACK);
        // }
        if self.recv_flag == true{
            return Ok(packet::Type::ACK);
        }

        if self.rtt != Duration::ZERO{
            return Ok(packet::Type::Application);
        }

        Err(Error::Done)
    }


    //read data from application
    pub fn data_send(& mut self, str_buf: & mut String){
        let mut output = str_buf.replace("\n", "");
        output = output.replace("\"", "");
        output = output.replace("\r","");
        let new_parts = output.split(">");
        self.norm2_vec.clear();
        self.send_data.clear();
        // let mut send_data:Vec<f32> = Vec::new();
        self.low_split_point = 0.0;
        self.high_split_point = 0.0;

        let mut norm2_tmp:Vec<f32>=Vec::new();
        for part in new_parts{
            if part == "" {
                break;
            }
            self.process_string(part.to_string(),&mut norm2_tmp);
        }
        norm2_tmp.sort_by(|a, b| a.partial_cmp(b).unwrap());
        self.low_split_point = norm2_tmp[norm2_tmp.len()*3/10 as usize];
        self.high_split_point = norm2_tmp[norm2_tmp.len()*7/10 as usize];
    }

    //store data and compute priority in advance
    pub fn process_string(& mut self, test_data: String, norm2_tmp: &mut Vec<f32>){
        let new_parts = test_data.split("[");
        let collection: Vec<&str> = new_parts.collect();
        let data = collection[1].to_string();
        let new_data = data.split("]");
        let collection: Vec<&str> = new_data.collect();
        let array_string = collection[0].to_string();
        let mut single_data: Vec<&str> = array_string.split(" ").collect();
        let white_space = "";
        single_data.retain(|&x| x != white_space);
    
        let mut norm2:f32 = 0.0;
        let mut it = 0;
        for item in single_data{
            it +=1;
            let my_float:f32 = FromStr::from_str(&item.to_string()).unwrap();
            norm2 +=my_float*my_float;
            let float_array = my_float.to_ne_bytes();
            self.send_data.extend(float_array);
            if it%256 == 255 {
                norm2_tmp.push(norm2);
                self.norm2_vec.push(norm2);
                norm2 = 0.0;
            }
        }
    
    }

}



#[derive(Clone, Debug, Eq, Default)]
pub struct RangeBuf {
    /// The internal buffer holding the data.
    ///
    /// To avoid needless allocations when a RangeBuf is split, this field is
    /// reference-counted and can be shared between multiple RangeBuf objects,
    /// and sliced using the `start` and `len` values.
    data: Vec<u8>,

    /// The initial offset within the internal buffer.
    start: usize,

    /// The current offset within the internal buffer.
    pos: usize,

    /// The number of bytes in the buffer, from the initial offset.
    len: usize,

    /// The offset of the buffer within a stream.
    off: u64,


}

impl RangeBuf {
    /// Creates a new `RangeBuf` from the given slice.
    pub fn from(buf: &[u8], off: u64) -> RangeBuf {
        RangeBuf {
            data: Vec::from(buf),
            start: 0,
            pos: 0,
            len: buf.len(),
            off,
        }
    }

    /// start refers to the start of all data.
    /// Returns the starting offset of `self`.
    /// Lowest offset of data, start == 0, pos == 0
    pub fn off(&self) -> u64 {
        (self.off - self.start as u64) + self.pos as u64
    }

    /// Returns the final offset of `self`.
    /// Returns the largest offset of the data
    pub fn max_off(&self) -> u64 {
        self.off() + self.len() as u64
    }

    /// Returns the length of `self`.
    pub fn len(&self) -> usize {
        // self.len - (self.pos - self.start)
        self.len - self.pos
    }

    /// Returns true if `self` has a length of zero bytes.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Consumes the starting `count` bytes of `self`.
    /// Consume() returns the max offset of the current data.
    pub fn consume(&mut self, count: usize) {
        self.pos += count;
    }

    /// Splits the buffer into two at the given index.
    pub fn split_off(&mut self, at: usize) -> RangeBuf {
        assert!(
            at <= self.len,
            "`at` split index (is {}) should be <= len (is {})",
            at,
            self.len
        );

        let buf = RangeBuf {
            data: self.data.clone()[at..].to_vec(),
            // start: self.start + at,
            // pos: cmp::max(self.pos, self.start + at),
            start: 0,
            pos: 0,
            len: self.len - at,
            off: self.off + at as u64,
        };

        self.pos = cmp::min(self.pos, self.start + at);
        self.len = at;

        buf
    }
}

impl std::ops::Deref for RangeBuf {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        &self.data[self.pos..self.start + self.len]
    }
}


impl PartialOrd for RangeBuf {
    fn partial_cmp(&self, other: &RangeBuf) -> Option<cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq for RangeBuf {
    fn eq(&self, other: &RangeBuf) -> bool {
        self.off == other.off
    }
}

/// Receive-side stream buffer.
///
/// Stream data received by the peer is buffered in a list of data chunks
/// ordered by offset in ascending order. Contiguous data can then be read
/// into a slice.
#[derive(Debug, Default)]
pub struct RecvBuf {
    /// Chunks of data received from the peer that have not yet been read by
    /// the application, ordered by offset.
    data: BTreeMap<u64, RangeBuf>,

    /// The lowest data offset that has yet to be read by the application.
    off: u64,

    /// The total length of data received on this stream.
    len: u64,

    last_maxoff: u64,
}

impl RecvBuf {
    /// Creates a new receive buffer.
    fn new() -> RecvBuf {
        RecvBuf {
            ..RecvBuf::default()
        }
    }

    /// Inserts the given chunk of data in the buffer.
    ///
    /// This also takes care of enforcing stream flow control limits, as well
    /// as handling incoming data that overlaps data that is already in the
    /// buffer.
    pub fn write(&mut self, out: &mut [u8], out_off: u64) -> Result<()> {
        let buf = RangeBuf::from(out,out_off);
        self.data.insert(buf.max_off(), buf);

        Ok(())
    }


    /// Writes data from the receive buffer into the given output buffer.
    ///
    /// Only contiguous data is written to the output buffer, starting from
    /// offset 0. The offset is incremented as data is read out of the receive
    /// buffer into the application buffer. If there is no data at the expected
    /// read offset, the `Done` error is returned.
    ///
    /// On success the amount of data read, and a flag indicating if there is
    /// no more data in the buffer, are returned as a tuple.
    /// out should be initialed as 0
    pub fn emit(&mut self, out: &mut [u8]) -> Result<usize> {
        let mut len:usize = 0;
        let mut cap = out.len();

        if !self.ready() {
            return Err(Error::Done);
        }

        // The stream was reset, so return the error code instead.
        // if let Some(e) = self.error {
        //     return Err(Error::StreamReset(e));
        // }

        while cap > 0 && self.ready() {
            let mut entry = match self.data.first_entry() {
                Some(entry) => entry,
                None => break,
            };

            let buf = entry.get_mut();

            // let mut zero_vec = Vec::<u8>::new();

            let max_off = buf.max_off();
            let data_len = buf.len();
            let mut zero_len = 0;
            if max_off - self.last_maxoff != data_len.try_into().unwrap(){
                zero_len = max_off - self.last_maxoff - (data_len as u64);
                // let value = 0;
                // zero_vec.extend(std::iter::repeat(value).take(zero_len.try_into().unwrap()));
            }
            

            /// 1. 0 can fill out the rest out buffer
            /// 2. 
            if zero_len < (cap as u64){
                cap -= zero_len as usize;
                len += zero_len as usize;
                self.last_maxoff += zero_len;
                let buf_len = cmp::min(buf.len(), cap); 
                out[len..len + buf_len].copy_from_slice(&buf.data[..buf_len]);
                self.last_maxoff += buf_len as u64;
                if buf_len < buf.len(){
                    buf.consume(buf_len);

                    // We reached the maximum capacity, so end here.
                    break;
                }
                entry.remove();
            }else if zero_len == cap as u64 {
                self.last_maxoff += zero_len;
                cap -= zero_len as usize;
                len += zero_len as usize;

                break;
            }else {
                self.last_maxoff += cap as u64;
                break;
            }
            
            // let buf_len = cmp::min(buf.len(), cap);

            // out[len..len + buf_len].copy_from_slice(&buf.data[..buf_len]);

            // self.off += buf_len as u64;

            // len += buf_len;
            // cap -= buf_len;

            // if buf_len < buf.len() {
            //     buf.consume(buf_len);

            //     // We reached the maximum capacity, so end here.
            //     break;
            // }

            // entry.remove();
        }


        Ok(len)
    }

    /// Resets the stream at the given offset.
    pub fn reset(&mut self, final_size: u64) -> Result<usize> {

        // Stream's known size is lower than data already received.
        if final_size < self.len {
            return Err(Error::FinalSize);
        }

        // Calculate how many bytes need to be removed from the connection flow
        // control.
        let max_data_delta = final_size - self.len;

        // Clear all data already buffered.
        self.off = final_size;

        self.data.clear();

       

        Ok(max_data_delta as usize)
    }


    /// Shuts down receiving data.
    pub fn shutdown(&mut self)  {
        self.data.clear();

    }

    /// Returns the lowest offset of data buffered.
    pub fn off_front(&self) -> u64 {
        self.off
    }


    /// Returns true if the stream has data to be read.
    fn ready(&self) -> bool {
        // let (_, buf) = match self.data.first_key_value() {
        //     Some(v) => v,
        //     None => return false,
        // };

        // buf.off() == self.off

        if self.data.is_empty(){
            return false
        }else {
            return true
        }
    }

    
}

/// Send-side stream buffer.
///
/// Stream data scheduled to be sent to the peer is buffered in a list of data
/// chunks ordered by offset in ascending order. Contiguous data can then be
/// read into a slice.
///
/// By default, new data is appended at the end of the stream, but data can be
/// inserted at the start of the buffer (this is to allow data that needs to be
/// retransmitted to be re-buffered).
#[derive(Debug, Default)]
pub struct SendBuf {
    /// Chunks of data to be sent, ordered by offset.
    data: VecDeque<RangeBuf>,

    // data:BTreeMap<u64, RangeBuf>,
    //retransmission buffer.
    retran_data: VecDeque<RangeBuf>,

    /// The index of the buffer that needs to be sent next.
    pos: usize,

    /// The maximum offset of data buffered in the stream.
    off: u64,

    /// The amount of data currently buffered.
    len: u64,

    /// The maximum offset we are allowed to send to the peer.
    /// 
    /// max_data sent in one window, which is equal to congestion window size
    max_data: u64,

    /// The error code received via STOP_SENDING.
    error: Option<u64>,


    /// retransmission data in sendbuf
    used_length: usize,

    
    // Ranges of data offsets that have been acked.
    // acked: ranges::RangeSet,
    offset_index: HashMap<u64,u64>,

    recv_index: Vec< bool>,

    index: u64,

    removed: u64,

    sent: usize,
}

impl SendBuf {
    /// Creates a new send buffer.
    /// max_data is equal to the congestion window size.
    fn new(max_data: u64) -> SendBuf {
        SendBuf {
            max_data,
            ..SendBuf::default()
        }
    }

    /// Returns the outgoing flow control capacity.
    pub fn cap(&self) -> Result<usize> {
        // The stream was stopped, so return the error code instead.
        if let Some(e) = self.error {
            return Err(Error::StreamStopped(e));
        }

        Ok((self.max_data - self.used_length as u64) as usize)
    }

    /// Inserts the given slice of data at the end of the buffer.
    ///
    /// The number of bytes that were actually stored in the buffer is returned
    /// (this may be lower than the size of the input buffer, in case of partial
    /// writes).
    /// write function is used to write new data into sendbuf, one congestion window 
    /// will run once.
    pub fn write(&mut self, mut data: &[u8], window_size: usize, off_len: usize) -> Result<usize> {
        self.recv_and_drop();
        self.max_data = window_size as u64;
        self.removed = 0;
        self.sent = 0;
        // Get the stream send capacity. This will return an error if the stream
        // was stopped.

        //Addressing left data is greater than the window size
        if self.len >= window_size.try_into().unwrap(){
            return Ok(0);
        }
    
        let capacity = self.cap()?;
        // self.used_length = window_size;
        // self.used_length = 0;

        if data.len() > capacity {
            // Truncate the input buffer according to the stream's capacity.
            let len = capacity;
            data = &data[..len];

        }

        // We already recorded the final offset, so we can just discard the
        // empty buffer now.
        if data.is_empty() {
            return Ok(data.len());
        }

        let mut len = 0;

        // Split function to set priority and message size
        // let mut split_result = split(data);

        /////
        if off_len > 0{
            let first_buf = RangeBuf::from(&data[..off_len], self.off);

            self.index += self.index+1;
            self.offset_index.insert( self.off,self.index);
            self.recv_index.push(false);

            self.data.push_back(first_buf);
            self.off += off_len as u64;
            self.len += off_len as u64;
            self.used_length += off_len;
            len += off_len;

        }     
        for chunk in data[off_len..].chunks(SEND_BUFFER_SIZE){

        // Split the remaining input data into consistently-sized buffers to
        // avoid fragmentation.
        //for chunk in data.chunks(SEND_BUFFER_SIZE) {
            
            len += chunk.len();
            self.index = self.index + 1;

            let buf = RangeBuf::from(chunk, self.off);
            
            self.offset_index.insert( self.off,self.index);
            self.recv_index.push(false);

            // The new data can simply be appended at the end of the send buffer.
            self.data.push_back(buf);

            self.off += chunk.len() as u64;
            self.len += chunk.len() as u64;
            self.used_length += chunk.len();
        }

        Ok(len)
    }

    /// Returns the lowest offset of data buffered.
    pub fn off_front(&self) -> u64 {
        let mut pos = self.pos;

        // Skip empty buffers from the start of the queue.
        while let Some(b) = self.data.get(pos) {
            if !b.is_empty() {
                return b.off();
            }
            pos += 1;
        }

        self.off
    }

    /// Returns true if there is data to be written.
    fn ready(&self) -> bool {
        // !self.data.is_empty() && self.off_front() < self.off
        !self.data.is_empty()
    }
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
    /// Writes data from the send buffer into the given output buffer.
    pub fn emit(&mut self, out: &mut [u8]) -> Result<(usize,u64,bool)> {
        let mut stop = false;
        let mut out_len = out.len();

        let out_off = self.off_front();

        let result_len = out.len();     

        while out_len >= SEND_BUFFER_SIZE && self.ready() 
        {
            let buf = match self.data.get_mut(self.pos) {
                Some(v) => v,

                None => break,
            };

            if buf.is_empty() {
                self.pos += 1;
                continue;
            }

            let buf_len = cmp::min(buf.len(), out_len);
            let partial = buf_len <= buf.len();

            // Copy data to the output buffer.
            // let out_pos = (next_off - out_off) as usize;
            let out_pos:usize = 0;
            out[out_pos..out_pos + buf_len].copy_from_slice(&buf.data[..buf_len]);

            self.len -= buf_len as u64;
            self.used_length -= buf_len;

            out_len = buf_len;

            buf.consume(buf_len);
            self.pos += 1;

            if partial {
                // We reached the maximum capacity, so end here.
                break;
            }

            //When sent data reach max congestion window size


        }
        self.sent += out_len;

        // result_len = out_len;

        //All data in the congestion control window has been sent. need to modify
        if self.sent  >= self.max_data.try_into().unwrap(){
            stop = true;
            // result_len = (self.max_data as usize)-self.sent;
            self.pos = 0;
        }
        Ok((out_len, out_off, stop))
    }

    /// Updates the max_data limit to the given value.
    pub fn update_max_data(&mut self, max_data: u64) {
        // self.max_data = cmp::max(self.max_data, max_data);
        self.max_data = max_data;
    }

    // pub fn recv_and_drop(&mut self, ack_set: &[u64], unack_set: &[u64],len: usize) {
    pub fn recv_and_drop(&mut self) {
        if self.data.is_empty() {
            return;
        }

        let mut iter = self.recv_index.iter();
        self.data.retain(|_| *iter.next().unwrap());

        self.recv_index.clear();
        // for i in ack_set{
        //     let index_off = self.offset_index.get(&i).unwrap() - self.removed;   
        //     if self.data.get(index_off.try_into().unwrap()).is_some(){
        //         let ack_len = self.data.get(index_off.try_into().unwrap()).unwrap().len();
        //         self.used_length = self.used_length - ack_len;   
        //         self.data.remove(index_off.try_into().unwrap());
        //         self.index -= 1;
        //         self.removed +=1;          
        //     }        
        // }
        // self.retransmit(unack_set);
  
    }


    pub fn ack_and_drop(&mut self, offset:u64){
        let index = self.offset_index.get(&offset).unwrap();
        self.recv_index[*index as usize]=true;
    }


    pub fn retransmit(&mut self, unack_set:&[u64]) {
        for i in unack_set{
            let position = self.pos;
            let b = self.data.get((self.offset_index.get(&i).unwrap() - self.removed).try_into().unwrap());  
            self.offset_index.entry(*i).or_insert(position.try_into().unwrap());   
            let index_off = self.offset_index.get(&i).unwrap() - self.removed;   
            self.data.remove(index_off.try_into().unwrap());
            self.index -= 1;
            self.removed +=1;
        }
    }


    ////rewritetv
    /// Resets the stream at the current offset and clears all buffered data.
    pub fn reset(&mut self) -> Result<u64> {
        let unsent_off = self.off_front();
        let unsent_len = self.off_back() - unsent_off;


        // Drop all buffered data.
        self.data.clear();

        self.pos = 0;
        self.len = 0;
        self.off = unsent_off;

        Ok(unsent_len)
    }

    pub fn clear(&mut self){
        self.data.clear();
        self.pos = 0;
        self.off = 0;
    }

    /// Returns the largest offset of data buffered.
    pub fn off_back(&self) -> u64 {
        self.off
    }

    /// The maximum offset we are allowed to send to the peer.
    pub fn max_off(&self) -> u64 {
        self.max_data 
    }

    /// Returns true if the stream was stopped before completion.
    pub fn is_stopped(&self) -> bool {
        self.error.is_some()
    }

}



mod recovery;
mod packet;
mod minmax;
use recovery::Recovery;

pub use crate::recovery::CongestionControlAlgorithm;
pub use crate::packet::Header;
pub use crate::packet::Type;
#[cfg(feature = "ffi")]
mod ffi;