﻿using FlatBuffers;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using xe.debug.proto;
using Xenia.Debug.Utilities;

namespace Xenia.Debug {

  public class Debugger {
    private readonly static string kServerHostname = "";
    private readonly static int kServerPort = 19000;

    public enum State {
      Idle,
      Attaching,
      Attached,
      Detached,
    }

    private class PendingRequest {
      public uint requestId;
      public ByteBuffer byteBuffer;
      public TaskCompletionSource<Response> responseTask;
    }

    private Socket socket;
    private readonly ConcurrentDictionary<uint, PendingRequest>
        pendingRequests = new ConcurrentDictionary<uint, PendingRequest>();
    private uint nextRequestId = 1;

    public event EventHandler<State> StateChanged;

    public State CurrentState {
      get; private set;
    }
    = State.Idle;

    public BreakpointList BreakpointList {
      get;
    }
    public FunctionList FunctionList {
      get;
    }
    public Memory Memory {
      get;
    }
    public ModuleList ModuleList {
      get;
    }
    public ThreadList ThreadList {
      get;
    }

    public Debugger(AsyncTaskRunner asyncTaskRunner) {
      Dispatch.AsyncTaskRunner = asyncTaskRunner;

      this.BreakpointList = new BreakpointList(this);
      this.FunctionList = new FunctionList(this);
      this.Memory = new Memory(this);
      this.ModuleList = new ModuleList(this);
      this.ThreadList = new ThreadList(this);
    }

    public Task Attach() {
      return Attach(CancellationToken.None);
    }

    public async Task Attach(CancellationToken cancellationToken) {
      System.Diagnostics.Debug.Assert(CurrentState == State.Idle);

      SocketPermission permission = new SocketPermission(
                NetworkAccess.Connect,
                TransportType.Tcp,
                kServerHostname,
                kServerPort
                );
      permission.Demand();

      IPAddress ipAddress = new IPAddress(new byte[] { 127, 0, 0, 1 });
      IPEndPoint ipEndPoint = new IPEndPoint(ipAddress, kServerPort);

      socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream,
                          ProtocolType.Tcp);
      socket.Blocking = false;
      socket.NoDelay = true;
      socket.ReceiveBufferSize = 1024 * 1024;
      socket.SendBufferSize = 1024 * 1024;
      socket.ReceiveTimeout = 0;
      socket.SendTimeout = 0;

      OnStateChanged(State.Attaching);

      while (true) {
        Task task = Task.Factory.FromAsync(socket.BeginConnect, socket.EndConnect, ipEndPoint, null);
        try {
          await task.WithCancellation(cancellationToken);
        } catch (OperationCanceledException) {
          socket.Close();
          socket = null;
          OnStateChanged(State.Idle);
          return;
        } catch (SocketException e) {
          if (e.SocketErrorCode == SocketError.ConnectionRefused) {
            // Not found - emulator may still be starting.
            System.Diagnostics.Debug.WriteLine("Connection refused; trying again...");
            continue;
          }
          OnStateChanged(State.Idle);
          return;
        }
        break;
      }

      // Start recv pump.
      Dispatch.Issue(() => ReceivePump());

      var fbb = BeginRequest();
      AttachRequest.StartAttachRequest(fbb);
      int requestDataOffset = AttachRequest.EndAttachRequest(fbb);
      var response = await CommitRequest(fbb, RequestData.AttachRequest, requestDataOffset);

      OnStateChanged(State.Attached);
    }

    public void Detach() {
      if (CurrentState == State.Idle || CurrentState == State.Detached) {
        return;
      }

      socket.Close();
      socket = null;

      OnStateChanged(State.Detached);
    }

    private async void ReceivePump() {
      // Read length.
      var lengthBuffer = new byte[4];
      int receiveLength;
      try {
        receiveLength = await Task.Factory.FromAsync(
            (callback, state) => socket.BeginReceive(lengthBuffer, 0, lengthBuffer.Length,
                                                     SocketFlags.None, callback, state),
            asyncResult => socket.EndReceive(asyncResult), null);
      } catch (SocketException) {
        System.Diagnostics.Debug.WriteLine("Socket read error; detaching");
        Detach();
        return;
      }
      if (receiveLength == 0 || receiveLength != 4) {
        // Failed?
        ReceivePump();
        return;
      }
      var length = BitConverter.ToInt32(lengthBuffer, 0);

      // Read body.
      var bodyBuffer = new byte[length];
      receiveLength = await Task.Factory.FromAsync(
          (callback, state) => socket.BeginReceive(bodyBuffer, 0, bodyBuffer.Length, SocketFlags.None, callback, state),
          asyncResult => socket.EndReceive(asyncResult),
          null);
      if (receiveLength == 0 || receiveLength != bodyBuffer.Length) {
        // Failed?
        ReceivePump();
        return;
      }

      // Emit message.
      OnMessageReceived(bodyBuffer);

      // Continue pumping.
      Dispatch.Issue(() => ReceivePump());
    }

    private void OnMessageReceived(byte[] buffer) {
      ByteBuffer byteBuffer = new ByteBuffer(buffer);
      var response = Response.GetRootAsResponse(byteBuffer);
      if (response.Id != 0) {
        // Response.
        PendingRequest pendingRequest;
        if (!pendingRequests.TryRemove(response.Id, out pendingRequest)) {
          System.Diagnostics.Debug.WriteLine("Unexpected message from debug server?");
          return;
        }
        pendingRequest.byteBuffer = byteBuffer;
        pendingRequest.responseTask.SetResult(response);
      } else {
        // Event.
        // TODO(benvanik): events.
      }
    }

    public FlatBufferBuilder BeginRequest() {
      var fbb = new FlatBufferBuilder(32 * 1024);
      return fbb;
    }

    public async Task<Response> CommitRequest(FlatBufferBuilder fbb,
                                              RequestData requestDataType,
                                              int requestDataOffset) {
      PendingRequest request = new PendingRequest();
      request.requestId = nextRequestId++;
      request.responseTask = new TaskCompletionSource<Response>();
      pendingRequests.TryAdd(request.requestId, request);

      int requestOffset =
          Request.CreateRequest(fbb, request.requestId,
                                requestDataType, requestDataOffset);
      fbb.Finish(requestOffset);

      // Update the placeholder size.
      int bufferOffset = fbb.DataBuffer.Position;
      int bufferLength = fbb.DataBuffer.Length - fbb.DataBuffer.Position;
      fbb.DataBuffer.PutInt(bufferOffset - 4, bufferLength);

      // Send request.
      await socket.SendTaskAsync(fbb.DataBuffer.Data, bufferOffset - 4,
                                 bufferLength + 4, SocketFlags.None);

      // Await response.
      var response = await request.responseTask.Task;

      return response;
    }

    private void OnStateChanged(State newState) {
      if (newState == CurrentState) {
        return;
      }
      CurrentState = newState;
      if (StateChanged != null) {
        StateChanged(this, newState);
      }
    }
  }
}
