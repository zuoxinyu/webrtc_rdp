/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

const SignalServer = 'http://10.10.10.133:8888';
let me = { name: 'webclient', id: '-1', online: false };
let currpeer = '';
let peers = [];

const peerList = document.getElementById('peerList');
const current = document.getElementById('current')
const myid = document.getElementById('myid')
const startButton = document.getElementById('startButton');
const callButton = document.getElementById('callButton');
const hangupButton = document.getElementById('hangupButton');


callButton.disabled = true;
hangupButton.disabled = true;
startButton.addEventListener('click', start);
callButton.addEventListener('click', call);
hangupButton.addEventListener('click', hangup);

let startTime;
const localVideo = document.getElementById('localVideo');
const remoteVideo = document.getElementById('remoteVideo');

localVideo.addEventListener('loadedmetadata', function () {
  console.log(`Local video videoWidth: ${this.videoWidth}px,  videoHeight: ${this.videoHeight}px`);
});

remoteVideo.addEventListener('loadedmetadata', function () {
  console.log(`Remote video videoWidth: ${this.videoWidth}px,  videoHeight: ${this.videoHeight}px`);
});

remoteVideo.addEventListener('resize', () => {
  console.log(`Remote video size changed to ${remoteVideo.videoWidth}x${remoteVideo.videoHeight} - Time since pageload ${performance.now().toFixed(0)}ms`);
  // We'll use the first onsize callback as an indication that video has started
  // playing out.
  if (startTime) {
    const elapsedTime = window.performance.now() - startTime;
    console.log('Setup time: ' + elapsedTime.toFixed(3) + 'ms');
    startTime = null;
  }
});

let localStream;
let pc1;
const offerOptions = {
  offerToReceiveAudio: 1,
  offerToReceiveVideo: 1
};

function udpatePeers() {
  peerList.innerHTML = '';
  peers.map(p => {
    const li = document.createElement('li');
    li.textContent = `${p.name} : ${p.id}`
    li.addEventListener('click', e => {
      currpeer = p.id
      current.innerText = p.id
    })
    return li;
  }).forEach(li => peerList.appendChild(li))
}

async function login() {
  const resp = await fetch(`${SignalServer}/sign_in`, {
    method: 'POST',
    body: JSON.stringify(me),
    headers: {
      'Pragma': '-1',
      'Content-Type': 'application/json',
      'Content-Length': JSON.stringify(me).length,
    },
  });
  console.log(resp)
  const json = await resp.json();
  me.online = true;
  me.id = resp.headers.get('Pragma');
  myid.innerText = me.id
  console.log('login resp:', json)
  peers = json;
  udpatePeers();
}

async function waitMessage() {
  const resp = await fetch(`${SignalServer}/wait?peer_id=${me.id}`, {
    method: 'GET',
    headers: {
      'Pragma': me.id,
    }
  });
  const json = await resp.json();
  peers = json.peers;
  udpatePeers();

  const msg = json.msg;

  if (!msg) {
    return;
  }

  switch (msg.type) {
    case 'offer':
      currpeer = msg.from;
      await onRemoteOffer(msg.payload);
      break;
    case 'answer':
      await onRemoteAnswer(msg.payload);
      break;
    case 'candidate':
      await onRemoteCandiate(msg.payload);
      break;
  }
}

async function sendMessage(to, typ, payload) {
  const msg = JSON.stringify({
    to: to,
    msg: {
      from: me.id,
      type: typ,
      payload: payload,
    }
  });
  await fetch(`${SignalServer}/send?peer_id=${me.id}`, {
    method: 'POST',
    body: msg,
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': msg.length,
      'Pragma': me.id,
    }
  })
}

async function start() {
  console.log('Requesting local stream');
  startButton.disabled = true;
  await login();

  setInterval(async function () {
    await waitMessage();
  }, 1000)

  try {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true, video: true });
    console.log('Received local stream');
    localVideo.srcObject = stream;
    localStream = stream;
    callButton.disabled = false;
  } catch (e) {
    alert(`getUserMedia() error: ${e.name}`);
  }

  callButton.disabled = false;
  hangupButton.disabled = false;
  console.log('Starting call');
  startTime = window.performance.now();
  const videoTracks = localStream.getVideoTracks();
  const audioTracks = localStream.getAudioTracks();
  if (videoTracks.length > 0) {
    console.log(`Using video device: ${videoTracks[0].label}`);
  }
  if (audioTracks.length > 0) {
    console.log(`Using audio device: ${audioTracks[0].label}`);
  }
  const configuration = {
    iceServers: [{
      urls: "stun:stun.services.mozilla.com",
    }, {
      urls: 'stun:stun1.l.google.com:19302',
    }],
  };
  console.log('RTCPeerConnection configuration:', configuration);
  pc1 = new RTCPeerConnection(configuration);
  console.log('Created local peer connection object pc1');
  pc1.addEventListener('icecandidate', onIceCandidate);
  pc1.addEventListener('track', onRemoteStream);

  localStream.getTracks().forEach(track => pc1.addTrack(track, localStream));
  console.log('Added local stream to pc1');
}

async function call() {
  try {
    console.log('pc1 createOffer start');
    const offer = await pc1.createOffer(offerOptions);
    await pc1.setLocalDescription(offer);
    await sendMessage(currpeer, 'offer', offer.sdp)
  } catch (e) {
    onCreateSessionDescriptionError(e);
  }
}

async function onRemoteOffer(offer) {
  try {
    await pc1.setRemoteDescription({ sdp: offer, type: 'offer' });
    const answer = await pc1.createAnswer();
    await pc1.setLocalDescription(answer);
    sendMessage(currpeer, 'answer', answer.sdp);
  } catch (e) {
    onCreateSessionDescriptionError(e)
  }
}

async function onRemoteAnswer(answer) {
  try {
    await pc1.setRemoteDescription({ sdp: answer, type: 'answer' });
  } catch (e) {
    onSetSessionDescriptionError(e);
  }
}

async function onRemoteCandiate(candi) {
  try {
    await pc1.addIceCandidate({ candidate: candi, sdpMid: '', sdpMLineIndex: 0 });
    onAddIceCandidateSuccess();
  } catch (e) {
    onAddIceCandidateError(e);
  }
}

function onCreateSessionDescriptionError(error) {
  console.log(`Failed to create session description: ${error.toString()}`);
}

function onRemoteStream(e) {
  if (remoteVideo.srcObject !== e.streams[0]) {
    remoteVideo.srcObject = e.streams[0];
    console.log('received remote stream');
  }
}

async function onIceCandidate(event) {
  await sendMessage(currpeer, 'candidate', event.candidate.candidate);
  console.log(`ICE candidate:\n${event.candidate ? event.candidate.candidate : '(null)'}`);
}

function onAddIceCandidateSuccess() {
  console.log(`addIceCandidate success`);
}

function onAddIceCandidateError(error) {
  console.log(`failed to add ICE Candidate: ${error.toString()}`);
}

function onSetSessionDescriptionError(error) {
  console.log(`failed to set sdp: ${error.toString()}`);
}

function onIceStateChange(event) {
  console.log(`ICE state: ${pc.iceConnectionState}`);
  console.log('ICE state change event: ', event);
}

function hangup() {
  console.log('Ending call');
  pc1.close();
  pc1 = null;
  hangupButton.disabled = true;
  callButton.disabled = false;
}
