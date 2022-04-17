function PlotFFT(Name,data,index)
    Fs = 250;
    T = 1/Fs;
    L = length(data);
	
	% If needed possible filters
    %data = lowpass(data,30,250);
    %data = bandpass(data,[1 30],250);
	
    four = fft(data);

    ABS = abs(four/L);

    P1 = ABS(1:L/2+1);

    P1(2:end-1) = 2*P1(2:end-1);

    f = Fs*(0:(L/2))/L;
    subplot(2,2,index);
    plot(f,P1);
    xlim([0 50]);
    ylim([0 100]);
    title(Name);

end