function PlotData(Name,data,index) % Plots the signaldata with a bandpass from 1-30Hz

    Fs = 250;
    T = 1/Fs;
    L = length(data);
    t = (0:L-1)*T;
    data = bandpass(data,[1 30],250);
    
    subplot(2,2,index);
    plot(t,data);
    title(Name);
    
end